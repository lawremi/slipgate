#include "MainWindow.h"

#include "CatalogService.h"
#include "EnvironmentDetector.h"
#include "Launcher.h"
#include "ModInstallService.h"
#include "QuakeData.h"
#include "SearchFilter.h"

#include <QApplication>
#include <QBoxLayout>
#include <QCheckBox>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QFrame>
#include <QLayout>
#include <QMessageBox>
#include <QPainter>
#include <QLocale>
#include <QProgressBar>
#include <QPushButton>
#include <QShortcut>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSplitter>
#include <QStyledItemDelegate>

namespace {

constexpr int DateRole = Qt::UserRole + 1;
constexpr int ChipsRole = Qt::UserRole + 2;
constexpr int AuthorsRole = Qt::UserRole + 3;
constexpr int InstalledRole = Qt::UserRole + 4;

QString stripHtml(QString html)
{
    html.replace(QRegularExpression("<[^>]*>"), "");
    html.replace("&quot;", "\"");
    html.replace("&amp;", "&");
    html.replace("&lt;", "<");
    html.replace("&gt;", ">");
    return html.trimmed();
}

QString htmlEscape(const QString &text)
{
    QString out = text.toHtmlEscaped();
    out.replace('\n', "<br>");
    return out;
}

QString paletteColor(QPalette::ColorRole role)
{
    return qApp->palette().color(role).name();
}

QString firstTagValue(const ModEntry &entry, const QString &field)
{
    return tagValue(entry.tags, field);
}

QStringList displayTypes(const ModEntry &entry)
{
    QStringList types = tagValues(entry.tags, "type");
    types.removeDuplicates();
    return types;
}

QStringList keywordChips(const ModEntry &entry)
{
    QStringList chips = displayTypes(entry);

    const QString mod = firstTagValue(entry, "mod");
    if (!mod.isEmpty()) {
        chips << "mod: " + mod;
    }

    const QStringList themes = tagValues(entry.tags, "theme");
    for (const QString &theme : themes.mid(0, 2)) {
        chips << "theme: " + theme;
    }

    chips.removeDuplicates();
    return chips;
}

QString engineHints(const ModEntry &entry)
{
    QStringList hints;
    const QString bsp = firstTagValue(entry, "bsp_format");
    if (!bsp.isEmpty()) {
        hints << bsp.toUpper();
    }
    const QString limits = firstTagValue(entry, "exceeds_quake_limits");
    if (limits.compare("yes", Qt::CaseInsensitive) == 0 || limits.compare("true", Qt::CaseInsensitive) == 0) {
        hints << "limit-removing";
    }
    const QString patch = firstTagValue(entry, "patch");
    if (!patch.isEmpty() && patch.compare("no", Qt::CaseInsensitive) != 0) {
        hints << "patch: " + patch;
    }
    return hints.join(", ");
}

class ModListDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);

        const QPalette palette = option.palette;
        const bool selected = option.state & QStyle::State_Selected;
        QRect card = option.rect.adjusted(4, 3, -4, -5);
        const QColor background = selected ? palette.highlight().color() : palette.base().color();
        const QColor border = palette.mid().color();
        const QColor text = selected ? palette.highlightedText().color() : palette.text().color();
        const QColor muted = selected ? palette.highlightedText().color().lighter(125) : palette.placeholderText().color();
        const QColor chipBg = selected ? palette.highlightedText().color().darker(140) : palette.alternateBase().color();

        painter->setPen(border);
        painter->setBrush(background);
        painter->drawRoundedRect(card, 8, 8);

        QRect content = card.adjusted(12, 8, -12, -10);
        QFont titleFont = option.font;
        titleFont.setBold(true);
        titleFont.setPointSize(titleFont.pointSize() + 1);
        painter->setFont(titleFont);
        painter->setPen(text);
        const QString title = index.data(Qt::DisplayRole).toString();
        const QString date = index.data(DateRole).toString();
        const bool installed = index.data(InstalledRole).toBool();
        int dateWidth = 0;
        if (!date.isEmpty()) {
            QFont dateFont = option.font;
            dateFont.setPointSize(qMax(8, dateFont.pointSize() - 1));
            QFontMetrics dateMetrics(dateFont);
            dateWidth = dateMetrics.horizontalAdvance(date) + 12;
            painter->setFont(dateFont);
            painter->setPen(muted);
            painter->drawText(QRect(content.right() - dateWidth, content.top(), dateWidth, 20),
                              Qt::AlignRight | Qt::AlignVCenter, date);
            painter->setFont(titleFont);
            painter->setPen(text);
        }
        painter->drawText(QRect(content.left(), content.top(), content.width() - dateWidth, 22),
                          Qt::AlignLeft | Qt::AlignTop,
                          painter->fontMetrics().elidedText(title, Qt::ElideRight, content.width() - dateWidth));

        if (installed) {
            const QRect indicator(card.right() - 20, card.bottom() - 20, 10, 10);
            painter->setPen(Qt::NoPen);
            painter->setBrush(selected ? palette.highlightedText().color() : palette.highlight().color());
            painter->drawEllipse(indicator);
        }

        int y = content.top() + painter->fontMetrics().height() + 5;
        QFont metaFont = option.font;
        metaFont.setPointSize(qMax(8, metaFont.pointSize() - 1));
        painter->setFont(metaFont);
        painter->setPen(muted);
        const QString authors = index.data(AuthorsRole).toString();
        if (!authors.isEmpty()) {
            const QString line = painter->fontMetrics().elidedText(authors, Qt::ElideRight, content.width());
            painter->drawText(QRect(content.left(), y, content.width(), 18), Qt::AlignLeft | Qt::AlignVCenter, line);
            y += 24;
        }

        const QStringList chips = index.data(ChipsRole).toStringList();
        int x = content.left();
        painter->setPen(Qt::NoPen);
        for (const QString &chip : chips.mid(0, 4)) {
            const QString label = painter->fontMetrics().elidedText(chip, Qt::ElideRight, 150);
            const int width = painter->fontMetrics().horizontalAdvance(label) + 18;
            if (x + width > content.right()) {
                break;
            }
            QRect chipRect(x, y, width, 22);
            painter->setBrush(chipBg);
            painter->drawRoundedRect(chipRect, 11, 11);
            painter->setPen(text);
            painter->drawText(chipRect.adjusted(9, 0, -9, 0), Qt::AlignCenter, label);
            painter->setPen(Qt::NoPen);
            x += width + 6;
        }

        painter->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const override
    {
        return QSize(320, 92);
    }
};

QLabel *makeChipLabel(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setMargin(0);
    label->setStyleSheet(
        "QLabel {"
        " border-radius: 10px;"
        " padding: 3px 9px;"
        " background: " + paletteColor(QPalette::AlternateBase) + ";"
        " color: " + paletteColor(QPalette::Text) + ";"
        " font-size: 12px;"
        "}"
    );
    return label;
}

void clearLayout(QLayout *layout)
{
    while (QLayoutItem *item = layout->takeAt(0)) {
        delete item->widget();
        delete item;
    }
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    catalogService_ = new CatalogService(this);
    installService_ = new ModInstallService(this);
    buildUi();

    connect(catalogService_, &CatalogService::refreshStarted, this, [this] {
        catalogLoading_ = true;
        updateActionPanel();
        setBusy(true, "Refreshing Quaddicted database...");
    });
    connect(catalogService_, &CatalogService::refreshFinished, this, [this](const QList<ModEntry> &entries) {
        catalogLoading_ = false;
        setBusy(false, {});
        catalog_ = entries;
        bySha_.clear();
        for (const ModEntry &entry : catalog_) {
            bySha_.insert(entry.sha256, entry);
        }
        applySearchFilter();
    });
    connect(catalogService_, &CatalogService::refreshFailed, this, [this](const QString &message) {
        catalogLoading_ = false;
        setBusy(false, {});
        setStatus(message);
    });

    connect(installService_, &ModInstallService::busyChanged, this, [this](bool busy, const QString &message) {
        setBusy(busy, message);
    });
    connect(installService_, &ModInstallService::statusChanged, this, [this](const QString &message) {
        setStatus(message);
        if (installService_->isBusy() && selectedEntry().sha256 == installService_->installingSha()) {
            actionInfoLabel_->setText(message);
        }
    });
    connect(installService_, &ModInstallService::progressChanged, this, [this](qint64 received, qint64 total) {
        if (total > 0) {
            progress_->setRange(0, static_cast<int>(total / 1024));
            progress_->setValue(static_cast<int>(received / 1024));
        } else if (installService_->isBusy()) {
            progress_->setRange(0, 0);
        }
    });
    connect(installService_, &ModInstallService::stateChanged, this, [this] {
        applySearchFilter();
        updateActionPanel();
    });

    detectEnvironment();
    refreshCatalog();
}

void MainWindow::buildUi()
{
    auto *central = new QWidget(this);
    auto *root = new QVBoxLayout(central);

    auto *headerRow = new QWidget(central);
    auto *headerLayout = new QHBoxLayout(headerRow);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    auto *title = new QLabel("Slipgate", headerRow);
    title->setStyleSheet("font-size: 26px; font-weight: 800; letter-spacing: 0.04em;");
    auto *subtitle = new QLabel("Quake launcher + Quaddicted browser", headerRow);
    subtitle->setStyleSheet("color: #6c6258;");
    auto *titleColumn = new QWidget(headerRow);
    auto *titleLayout = new QVBoxLayout(titleColumn);
    titleLayout->setContentsMargins(0, 0, 0, 0);
    titleLayout->setSpacing(0);
    titleLayout->addWidget(title);
    titleLayout->addWidget(subtitle);
    auto *refreshButton = new QPushButton("Refresh Database", headerRow);
    auto *openDownloads = new QPushButton("Open Downloads", headerRow);
    headerLayout->addWidget(titleColumn, 1);
    headerLayout->addWidget(refreshButton);
    headerLayout->addWidget(openDownloads);
    root->addWidget(headerRow);

    settingsToggleButton_ = new QPushButton("Show Settings", central);
    settingsToggleButton_->setFlat(true);
    root->addWidget(settingsToggleButton_, 0, Qt::AlignLeft);

    settingsGroup_ = new QGroupBox("Game setup", central);
    auto *form = new QFormLayout(settingsGroup_);
    quakeDirEdit_ = new QLineEdit(settingsGroup_);
    clientEdit_ = new QLineEdit(settingsGroup_);

    auto *quakeRow = new QWidget(settingsGroup_);
    auto *quakeLayout = new QHBoxLayout(quakeRow);
    quakeLayout->setContentsMargins(0, 0, 0, 0);
    quakeLayout->addWidget(quakeDirEdit_);
    auto *browseQuake = new QPushButton("Browse", quakeRow);
    quakeLayout->addWidget(browseQuake);

    auto *clientRow = new QWidget(settingsGroup_);
    auto *clientLayout = new QHBoxLayout(clientRow);
    clientLayout->setContentsMargins(0, 0, 0, 0);
    clientLayout->addWidget(clientEdit_);
    auto *browseClient = new QPushButton("Browse", clientRow);
    clientLayout->addWidget(browseClient);

    form->addRow("Quake folder", quakeRow);
    form->addRow("Client executable", clientRow);
    quakeDataStatusLabel_ = new QLabel(settingsGroup_);
    quakeDataStatusLabel_->setWordWrap(true);
    pakSymlinkButton_ = new QPushButton("Create lowercase pak symlinks", settingsGroup_);
    pakSymlinkButton_->hide();
    auto *dataRow = new QWidget(settingsGroup_);
    auto *dataLayout = new QHBoxLayout(dataRow);
    dataLayout->setContentsMargins(0, 0, 0, 0);
    dataLayout->addWidget(quakeDataStatusLabel_, 1);
    dataLayout->addWidget(pakSymlinkButton_);
    form->addRow("Base game data", dataRow);
    root->addWidget(settingsGroup_);
    setSettingsExpanded(false);

    auto *searchRow = new QWidget(central);
    auto *searchLayout = new QHBoxLayout(searchRow);
    searchLayout->setContentsMargins(0, 0, 0, 0);
    searchEdit_ = new QLineEdit(searchRow);
    searchEdit_->setPlaceholderText("Search titles, or use field:value like author:sock type:episode");
    searchLayout->addWidget(searchEdit_, 1);
    root->addWidget(searchRow);

    auto *splitter = new QSplitter(central);
    list_ = new QListWidget(splitter);
    list_->setAlternatingRowColors(true);
    list_->setUniformItemSizes(false);
    list_->setSpacing(4);
    list_->setItemDelegate(new ModListDelegate(list_));

    auto *detailPane = new QWidget(splitter);
    auto *detailLayout = new QVBoxLayout(detailPane);
    detailLayout->setContentsMargins(0, 0, 0, 0);
    detailLayout->setSpacing(8);
    auto *detailScroll = new QScrollArea(detailPane);
    detailScroll->setWidgetResizable(true);
    detailScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    detailScroll->setFrameShape(QFrame::NoFrame);
    auto *detailFrame = new QFrame(detailScroll);
    detailFrame->setFrameShape(QFrame::StyledPanel);
    details_ = detailFrame;
    auto *detailsLayout = new QVBoxLayout(detailFrame);
    detailsLayout->setContentsMargins(14, 14, 14, 14);
    detailsLayout->setSpacing(10);

    auto *titleRow = new QWidget(detailFrame);
    auto *detailTitleLayout = new QHBoxLayout(titleRow);
    detailTitleLayout->setContentsMargins(0, 0, 0, 0);
    detailTitleLayout->setSpacing(12);
    detailTitleLabel_ = new QLabel(titleRow);
    detailTitleLabel_->setWordWrap(true);
    detailTitleLabel_->setStyleSheet("font-size: 28px; font-weight: 700;");
    detailDateLabel_ = new QLabel(titleRow);
    detailDateLabel_->setAlignment(Qt::AlignRight | Qt::AlignBottom);
    detailDateLabel_->setStyleSheet("font-size: 13px;");
    detailTitleLayout->addWidget(detailTitleLabel_, 1);
    detailTitleLayout->addWidget(detailDateLabel_, 0, Qt::AlignBottom);
    detailsLayout->addWidget(titleRow);

    detailAuthorsLabel_ = new QLabel(detailFrame);
    detailAuthorsLabel_->setWordWrap(true);
    detailAuthorsLabel_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    detailAuthorsLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    detailsLayout->addWidget(detailAuthorsLabel_);

    detailChipsWidget_ = new QWidget(detailFrame);
    auto *chipsLayout = new QHBoxLayout(detailChipsWidget_);
    chipsLayout->setContentsMargins(0, 0, 0, 0);
    chipsLayout->setSpacing(6);
    chipsLayout->addStretch(1);
    detailsLayout->addWidget(detailChipsWidget_);

    detailDescriptionLabel_ = new QLabel(detailFrame);
    detailDescriptionLabel_->setWordWrap(true);
    detailDescriptionLabel_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    detailDescriptionLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    detailDescriptionLabel_->setStyleSheet("font-size: 15px;");
    detailsLayout->addWidget(detailDescriptionLabel_);

    detailMetaLabel_ = new QLabel(detailFrame);
    detailMetaLabel_->setTextFormat(Qt::RichText);
    detailMetaLabel_->setWordWrap(true);
    detailMetaLabel_->setOpenExternalLinks(true);
    detailMetaLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::LinksAccessibleByMouse);
    detailsLayout->addWidget(detailMetaLabel_);
    detailsLayout->addStretch(1);

    detailScroll->setWidget(detailFrame);
    detailLayout->addWidget(detailScroll, 1);

    actionPanel_ = new QWidget(detailPane);
    auto *actionLayout = new QHBoxLayout(actionPanel_);
    actionLayout->setContentsMargins(0, 0, 0, 0);
    installButton_ = new QPushButton("Download && Install", actionPanel_);
    uninstallButton_ = new QPushButton("Uninstall", actionPanel_);
    deleteArchiveButton_ = new QPushButton("Delete Archive", actionPanel_);
    loadLatestSaveCheck_ = new QCheckBox("Load latest save", actionPanel_);
    loadLatestSaveCheck_->setToolTip("Launch with the newest .sav file for this mod instead of starting at the listed start map.");
    launchButton_ = new QPushButton("Launch", actionPanel_);
    progress_ = new QProgressBar(actionPanel_);
    progress_->setRange(0, 1);
    progress_->setValue(0);
    progress_->hide();
    actionInfoLabel_ = new QLabel(actionPanel_);
    actionInfoLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    actionInfoLabel_->setWordWrap(true);
    actionLayout->addWidget(installButton_);
    actionLayout->addWidget(uninstallButton_);
    actionLayout->addWidget(deleteArchiveButton_);
    actionLayout->addWidget(progress_, 1);
    actionLayout->addWidget(actionInfoLabel_, 1);
    actionLayout->addWidget(loadLatestSaveCheck_);
    actionLayout->addWidget(launchButton_);
    detailLayout->addWidget(actionPanel_);

    splitter->addWidget(list_);
    splitter->addWidget(detailPane);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 2);
    splitter->setSizes({360, 720});
    root->addWidget(splitter, 1);

    auto *statusRow = new QWidget(central);
    auto *statusLayout = new QHBoxLayout(statusRow);
    statusLayout->setContentsMargins(0, 0, 0, 0);
    statusLabel_ = new QLabel(statusRow);
    statusLayout->addWidget(statusLabel_, 1);
    statusProgress_ = new QProgressBar(statusRow);
    statusProgress_->setRange(0, 0);
    statusProgress_->hide();
    statusLayout->addWidget(statusProgress_);
    root->addWidget(statusRow);

    setCentralWidget(central);
    resize(1120, 720);
    setWindowTitle("Slipgate");

    connect(browseQuake, &QPushButton::clicked, this, [this] {
        const QString dir = QFileDialog::getExistingDirectory(this, "Choose Quake folder", quakeDirEdit_->text());
        if (!dir.isEmpty()) {
            quakeDirEdit_->setText(dir);
            validateQuakeDir();
        }
    });
    connect(quakeDirEdit_, &QLineEdit::editingFinished, this, [this] { validateQuakeDir(); });
    connect(browseClient, &QPushButton::clicked, this, [this] {
        const QString file = QFileDialog::getOpenFileName(this, "Choose Quake client", clientEdit_->text());
        if (!file.isEmpty()) {
            clientEdit_->setText(file);
        }
    });
    connect(refreshButton, &QPushButton::clicked, this, [this] { refreshCatalog(); });
    connect(settingsToggleButton_, &QPushButton::clicked, this, [this] {
        setSettingsExpanded(!settingsGroup_->isVisible());
    });
    connect(searchEdit_, &QLineEdit::textChanged, this, [this] { applySearchFilter(); });
    connect(searchEdit_, &QLineEdit::returnPressed, this, [this] { applySearchFilter(); });
    connect(list_, &QListWidget::itemActivated, this, [this] { activateSelected(); });
    connect(list_, &QListWidget::currentItemChanged, this, [this] { showSelectedDetails(); });
    connect(installButton_, &QPushButton::clicked, this, [this] { installSelected(); });
    connect(uninstallButton_, &QPushButton::clicked, this, [this] { uninstallSelected(); });
    connect(deleteArchiveButton_, &QPushButton::clicked, this, [this] { deleteSelectedArchive(); });
    connect(pakSymlinkButton_, &QPushButton::clicked, this, [this] { createPakSymlinks(); });
    connect(launchButton_, &QPushButton::clicked, this, [this] { launchSelected(); });
    connect(openDownloads, &QPushButton::clicked, this, [this] {
        installService_->ensureDownloadsDir();
        QDesktopServices::openUrl(QUrl::fromLocalFile(installService_->downloadsPath()));
    });
    auto *installShortcut = new QShortcut(QKeySequence(Qt::Key_Return), list_);
    installShortcut->setContext(Qt::WidgetShortcut);
    connect(installShortcut, &QShortcut::activated, this, [this] { activateSelected(); });
    auto *installEnterShortcut = new QShortcut(QKeySequence(Qt::Key_Enter), list_);
    installEnterShortcut->setContext(Qt::WidgetShortcut);
    connect(installEnterShortcut, &QShortcut::activated, this, [this] { activateSelected(); });
    updateActionPanel();
}

void MainWindow::detectEnvironment()
{
    const DetectedEnvironment env = EnvironmentDetector::detect();
    quakeDirEdit_->setText(env.quakeDir);
    clientEdit_->setText(env.clientExecutable);
    setStatus(env.notes.join("  "));
    validateQuakeDir();
}

void MainWindow::validateQuakeDir()
{
    const QString quakeDir = quakeDirEdit_->text().trimmed();
    const PakValidation validation = validatePaks(quakeDir);
    canCreatePakSymlinks_ = false;
    quakeDataValid_ = false;
    auto setError = [this](const QString &message) {
        quakeDataStatusLabel_->setStyleSheet("color: #c43b3b; font-weight: 600;");
        quakeDataStatusLabel_->setText(message);
    };
    auto setOk = [this](const QString &message) {
        quakeDataStatusLabel_->setStyleSheet("");
        quakeDataStatusLabel_->setText(message);
    };

    if (quakeDir.isEmpty() || !QDir(quakeDir).exists()) {
        setError("Choose a Quake folder. Missing id1/pak0.pak and id1/pak1.pak.");
        pakSymlinkButton_->hide();
        setSettingsExpanded(true);
        return;
    }
    if (!validation.id1Exists) {
        setError("Missing id1 directory and required files: id1/pak0.pak, id1/pak1.pak.");
        pakSymlinkButton_->hide();
        setSettingsExpanded(true);
        return;
    }
    if (validation.lowercasePak0 && validation.lowercasePak1) {
        quakeDataValid_ = true;
        setOk("Found id1/pak0.pak and id1/pak1.pak.");
        pakSymlinkButton_->hide();
        return;
    }

    QStringList missing;
    if (!validation.lowercasePak0) {
        missing << "id1/pak0.pak";
    }
    if (!validation.lowercasePak1) {
        missing << "id1/pak1.pak";
    }

#ifdef Q_OS_LINUX
    if (validation.uppercasePak0 && validation.uppercasePak1) {
        setError("Missing lowercase files: " + missing.join(", ") + ". Steam has uppercase PAK files; create lowercase symlinks.");
        canCreatePakSymlinks_ = true;
        pakSymlinkButton_->show();
        setSettingsExpanded(true);
        return;
    }
#endif

    setError("Missing required files: " + missing.join(", ") + ".");
    pakSymlinkButton_->hide();
    setSettingsExpanded(true);
}

void MainWindow::setSettingsExpanded(bool expanded)
{
    if (!settingsGroup_ || !settingsToggleButton_) {
        return;
    }
    settingsGroup_->setVisible(expanded);
    settingsToggleButton_->setText(expanded ? "Hide Settings" : "Show Settings");
}

void MainWindow::createPakSymlinks()
{
    if (!canCreatePakSymlinks_) {
        validateQuakeDir();
        return;
    }

    const bool ok = createLowercasePakSymlinks(quakeDirEdit_->text().trimmed());
    validateQuakeDir();
    setStatus(ok ? "Created lowercase pak symlinks." : "Could not create one or more pak symlinks.");
}

void MainWindow::refreshCatalog()
{
    catalogService_->refresh();
}

void MainWindow::applySearchFilter()
{
    const QString query = searchEdit_->text();
    const QList<ModEntry> entries = filterAndSortEntries(catalog_, query, installService_->state().installedSha);

    showEntries(entries);
    if (query.trimmed().isEmpty()) {
        setStatus(QString("Loaded %1 Quaddicted entries.").arg(catalog_.size()));
    } else {
        setStatus(QString("Showing %1 of %2 entries.").arg(entries.size()).arg(catalog_.size()));
    }
}

void MainWindow::showEntries(const QList<ModEntry> &entries)
{
    const QString selectedSha = list_->currentItem() ? list_->currentItem()->data(Qt::UserRole).toString() : QString();
    list_->clear();

    int selectedRow = -1;
    for (const ModEntry &entry : entries) {
        auto *item = new QListWidgetItem(entry.displayTitle(), list_);
        item->setData(Qt::UserRole, entry.sha256);
        item->setData(DateRole, entry.releaseDate);
        item->setData(AuthorsRole, entry.authors.join(", "));
        item->setData(ChipsRole, keywordChips(entry));
        item->setData(InstalledRole, installService_->isInstalled(entry.sha256));
        item->setToolTip(stripHtml(entry.descriptionHtml));
        item->setSizeHint(QSize(item->sizeHint().width(), 92));
        if (entry.sha256 == selectedSha) {
            selectedRow = list_->count() - 1;
        }
    }

    if (selectedRow >= 0) {
        list_->setCurrentRow(selectedRow);
    } else if (list_->count() > 0) {
        list_->setCurrentRow(0);
    } else {
        detailTitleLabel_->clear();
        detailDateLabel_->clear();
        detailAuthorsLabel_->clear();
        detailDescriptionLabel_->clear();
        detailMetaLabel_->clear();
        clearLayout(detailChipsWidget_->layout());
        detailChipsWidget_->layout()->addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum));
    }
    updateActionPanel();
}

void MainWindow::showSelectedDetails()
{
    const ModEntry entry = selectedEntry();
    if (entry.sha256.isEmpty()) {
        detailTitleLabel_->clear();
        detailDateLabel_->clear();
        detailAuthorsLabel_->clear();
        detailDescriptionLabel_->clear();
        detailMetaLabel_->clear();
        clearLayout(detailChipsWidget_->layout());
        detailChipsWidget_->layout()->addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum));
        updateActionPanel();
        return;
    }

    const QString muted = paletteColor(QPalette::PlaceholderText);
    detailTitleLabel_->setText(entry.displayTitle());
    detailDateLabel_->setText(entry.releaseDate);
    detailDateLabel_->setStyleSheet("font-size: 13px; color: " + muted + ";");
    detailAuthorsLabel_->setText(entry.authors.join(", "));
    detailAuthorsLabel_->setStyleSheet("color: " + muted + ";");

    clearLayout(detailChipsWidget_->layout());
    for (const QString &keyword : keywordChips(entry).mid(0, 6)) {
        detailChipsWidget_->layout()->addWidget(makeChipLabel(keyword, detailChipsWidget_));
    }
    detailChipsWidget_->layout()->addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum));

    detailDescriptionLabel_->setText(stripHtml(entry.descriptionHtml));

    auto linkHtml = [](const QStringList &links) {
        QStringList anchors;
        for (const QString &link : links) {
            const QStringList parts = link.split('\t');
            if (parts.size() >= 2) {
                anchors << "<a href=\"" + htmlEscape(parts.at(1)) + "\">" + htmlEscape(parts.at(0)) + "</a>";
            }
        }
        return anchors.join(" &nbsp; ");
    };

    QString metaHtml = "<table style=\"border-collapse:collapse; width:100%;\">";
    auto addRow = [&metaHtml](const QString &label, const QString &value) {
        if (value.isEmpty()) {
            return;
        }
        metaHtml += "<tr>";
        metaHtml += "<td style=\"vertical-align:top; color:" + paletteColor(QPalette::PlaceholderText) +
                "; padding:5px 14px 5px 0; white-space:nowrap;\">" +
                htmlEscape(label) + "</td>";
        metaHtml += "<td style=\"vertical-align:top; padding:5px 0;\">" + htmlEscape(value) + "</td>";
        metaHtml += "</tr>";
    };
    if (!entry.commandLine.isEmpty()) {
        addRow("Command line", entry.commandLine);
    }
    if (!entry.primaryStartMap().isEmpty()) {
        addRow("Start map", entry.primaryStartMap());
    }
    if (!entry.startMaps.isEmpty()) {
        addRow("Maps", entry.startMaps.join(", "));
    }
    if (!entry.dependencies.isEmpty()) {
        addRow("Dependencies", entry.dependencies.join(", "));
    }
    if (!entry.filename.isEmpty()) {
        addRow("Archive", entry.filename);
    }
    if (entry.bytes > 0) {
        addRow("Size", QLocale().formattedDataSize(entry.bytes));
    }
    addRow("Version", firstTagValue(entry, "version"));
    addRow("Release group", firstTagValue(entry, "release_group"));
    addRow("Engine notes", engineHints(entry));
    if (!entry.links.isEmpty()) {
        metaHtml += "<tr>";
        metaHtml += "<td style=\"vertical-align:top; color:" + paletteColor(QPalette::PlaceholderText) +
                    "; padding:5px 14px 5px 0; white-space:nowrap;\">Links</td>";
        metaHtml += "<td style=\"vertical-align:top; padding:5px 0;\">" + linkHtml(entry.links) + "</td>";
        metaHtml += "</tr>";
    }
    metaHtml += "</table>";
    detailMetaLabel_->setText(metaHtml);
    updateActionPanel();
}

void MainWindow::updateActionPanel()
{
    const ModEntry entry = selectedEntry();
    const bool hasSelection = !entry.sha256.isEmpty();
    const bool installingThis = hasSelection && installService_->installingSha() == entry.sha256 && installService_->isBusy();
    const bool installed = hasSelection && installService_->isInstalled(entry.sha256);
    const bool archiveCached = hasSelection && installService_->archiveExists(entry);

    actionPanel_->setVisible(!catalogLoading_ && hasSelection);
    if (catalogLoading_ || !hasSelection) {
        progress_->hide();
        actionInfoLabel_->clear();
        return;
    }

    installButton_->setVisible(hasSelection);
    installButton_->setEnabled(hasSelection && !installingThis);
    installButton_->setText(installed ? "Reinstall" : (archiveCached ? "Install" : "Download && Install"));

    launchButton_->setVisible(installingThis || installed);
    launchButton_->setEnabled(installed && !installingThis);
    loadLatestSaveCheck_->setVisible(installed && !installingThis);
    loadLatestSaveCheck_->setEnabled(installed && !installingThis);
    uninstallButton_->setVisible(installed && !installingThis);
    uninstallButton_->setEnabled(installed && !installingThis);
    deleteArchiveButton_->setVisible(archiveCached && !installingThis);
    deleteArchiveButton_->setEnabled(archiveCached && !installingThis);

    if (installingThis) {
        actionInfoLabel_->setText("Installing " + entry.displayTitle() + "...");
        return;
    }

    progress_->hide();
    if (installed) {
        actionInfoLabel_->setText("Installed " + installService_->state().installedAt.value(entry.sha256) + ". Ready to launch.");
    } else {
        actionInfoLabel_->clear();
    }
}

ModEntry MainWindow::selectedEntry() const
{
    const QListWidgetItem *item = list_->currentItem();
    if (!item) {
        return {};
    }
    return bySha_.value(item->data(Qt::UserRole).toString());
}

void MainWindow::setBusy(bool busy, const QString &message)
{
    const bool mapBusy = busy && !installService_->installingSha().isEmpty();
    const bool statusBusy = busy && installService_->installingSha().isEmpty();
    progress_->setVisible(mapBusy);
    progress_->setRange(mapBusy ? 0 : 0, mapBusy ? 0 : 1);
    statusProgress_->setVisible(statusBusy);
    statusProgress_->setRange(0, statusBusy ? 0 : 1);
    installButton_->setEnabled(!busy);
    uninstallButton_->setEnabled(!busy && installService_->isInstalled(selectedEntry().sha256));
    launchButton_->setEnabled(!busy && installService_->isInstalled(selectedEntry().sha256));
    if (!message.isEmpty()) {
        setStatus(message);
    }
    if (!busy) {
        updateActionPanel();
    }
}

void MainWindow::setStatus(const QString &message)
{
    statusLabel_->setText(message);
}

void MainWindow::installSelected()
{
    const ModEntry entry = selectedEntry();
    if (entry.sha256.isEmpty()) {
        setStatus("Choose a mod first.");
        return;
    }
    if (quakeDirEdit_->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "Missing Quake folder", "Choose your Quake folder before installing.");
        return;
    }

    actionInfoLabel_->setText("Resolving dependencies for " + entry.displayTitle() + "...");
    installService_->installWithDependencies(entry, catalog_, quakeDirEdit_->text().trimmed());
}

void MainWindow::uninstallSelected()
{
    const ModEntry entry = selectedEntry();
    if (entry.sha256.isEmpty() || !installService_->isInstalled(entry.sha256)) {
        return;
    }

    const auto answer = QMessageBox::question(
        this,
        "Uninstall " + entry.displayTitle(),
        "Remove files Slipgate installed for this entry? Shared dependencies are left alone."
    );
    if (answer != QMessageBox::Yes) {
        return;
    }

    installService_->uninstall(entry, quakeDirEdit_->text().trimmed());
}

void MainWindow::deleteSelectedArchive()
{
    const ModEntry entry = selectedEntry();
    if (entry.sha256.isEmpty()) {
        return;
    }

    if (!installService_->archiveExists(entry)) {
        updateActionPanel();
        return;
    }

    if (!installService_->archiveChecksumValid(entry)) {
        const auto answer = QMessageBox::question(
            this,
            "Delete Archive",
            "The cached archive does not match the expected checksum. Delete it anyway?"
        );
        if (answer != QMessageBox::Yes) {
            return;
        }
    }

    installService_->deleteArchive(entry);
}

void MainWindow::activateSelected()
{
    const ModEntry entry = selectedEntry();
    if (entry.sha256.isEmpty()) {
        return;
    }

    if (installService_->isInstalled(entry.sha256)) {
        launchWithEntry(entry);
    } else {
        installSelected();
    }
}

void MainWindow::launchSelected()
{
    const ModEntry entry = selectedEntry();
    if (entry.sha256.isEmpty()) {
        setStatus("Choose a mod first.");
        return;
    }
    validateQuakeDir();
    if (!quakeDataValid_) {
        QMessageBox::warning(
            this,
            "Missing Quake Data",
            "Cannot launch until the Quake folder contains lowercase id1/pak0.pak and id1/pak1.pak."
        );
        return;
    }
    launchWithEntry(entry);
}

void MainWindow::launchWithEntry(const ModEntry &entry)
{
    const QString quakeDir = quakeDirEdit_->text().trimmed();
    const QString client = clientEdit_->text().trimmed();
    const LaunchOptions options{loadLatestSaveCheck_->isChecked()};
    QString error;
    if (!launchMod(client, quakeDir, entry, options, &error)) {
        QMessageBox::warning(this, options.loadLatestSave ? "No save found" : "No launcher found", error);
        return;
    }
    setStatus(options.loadLatestSave ? "Launched latest save for " + entry.displayTitle() : "Launched " + entry.displayTitle());
}
