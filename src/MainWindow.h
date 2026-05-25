#pragma once

#include "ModEntry.h"

#include <QHash>
#include <QMainWindow>

class CatalogService;
class QCheckBox;
class QLabel;
class QLineEdit;
class QGroupBox;
class QListWidget;
class QListWidgetItem;
class ModInstallService;
class QProgressBar;
class QPushButton;
class QWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private:
    void buildUi();
    void detectEnvironment();
    void validateQuakeDir();
    void createPakSymlinks();
    void setSettingsExpanded(bool expanded);
    void refreshCatalog();
    void applySearchFilter();
    void showEntries(const QList<ModEntry> &entries);
    void showSelectedDetails();
    void updateActionPanel();
    void setBusy(bool busy, const QString &message);
    void setStatus(const QString &message);
    void installSelected();
    void uninstallSelected();
    void deleteSelectedArchive();
    void activateSelected();
    void launchSelected();
    void launchWithEntry(const ModEntry &entry);

    [[nodiscard]] ModEntry selectedEntry() const;

    CatalogService *catalogService_ = nullptr;
    ModInstallService *installService_ = nullptr;
    QList<ModEntry> catalog_;
    QHash<QString, ModEntry> bySha_;

    QLineEdit *quakeDirEdit_ = nullptr;
    QLineEdit *clientEdit_ = nullptr;
    QGroupBox *settingsGroup_ = nullptr;
    QPushButton *settingsToggleButton_ = nullptr;
    QLabel *quakeDataStatusLabel_ = nullptr;
    QPushButton *pakSymlinkButton_ = nullptr;
    QLineEdit *searchEdit_ = nullptr;
    QListWidget *list_ = nullptr;
    QWidget *details_ = nullptr;
    QLabel *detailTitleLabel_ = nullptr;
    QLabel *detailDateLabel_ = nullptr;
    QLabel *detailAuthorsLabel_ = nullptr;
    QWidget *detailChipsWidget_ = nullptr;
    QLabel *detailDescriptionLabel_ = nullptr;
    QLabel *detailMetaLabel_ = nullptr;
    QWidget *actionPanel_ = nullptr;
    QLabel *actionInfoLabel_ = nullptr;
    QLabel *statusLabel_ = nullptr;
    QProgressBar *statusProgress_ = nullptr;
    QProgressBar *progress_ = nullptr;
    QPushButton *installButton_ = nullptr;
    QPushButton *uninstallButton_ = nullptr;
    QPushButton *deleteArchiveButton_ = nullptr;
    QCheckBox *loadLatestSaveCheck_ = nullptr;
    QPushButton *launchButton_ = nullptr;

    bool catalogLoading_ = false;
    bool canCreatePakSymlinks_ = false;
    bool quakeDataValid_ = false;
};
