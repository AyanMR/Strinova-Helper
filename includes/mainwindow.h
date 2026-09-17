//
// Created by AyanMR on 26-3-1.
//

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QButtonGroup>
#include <QComboBox>
#include <QEvent>
#include <QHash>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QTreeWidget>
#include <QWidget>
#include <QJsonObject>
#include <QStringList>
#include <future>

extern int refresh_rate;

QT_BEGIN_NAMESPACE

namespace Ui
{
    class MainWindow;
}

QT_END_NAMESPACE

class MainWindow : public QWidget
{
        Q_OBJECT

    public:
        explicit MainWindow(QWidget *parent = nullptr);

        ~MainWindow() override = default;

    private slots:
        void refreshBalanceData();

    private:
        void populateDataTree(const QJsonObject &cacheDocument);
        QTreeWidgetItem *createDataItem(const QJsonObject &row, const QString &categoryLabel) const;
        void finishBalanceDataRefresh();
        void applyFilters();
        void ensureCurrentSelectionData();
        QStringList selectedRankCodes() const;
        QStringList selectedModeCodes() const;
        void toggleRankOption(int index);
        void updateRankSummary();

    protected:
        bool eventFilter(QObject *watched, QEvent *event) override;

    private:
        void populateFilterOptions(const QJsonObject &cacheDocument);
        void setRefreshProgress(int progress, const QString &message);

        QWidget *titleBar;
        QTreeWidget *dataTree;
        QLabel *statusLabel;
        QPushButton *refreshButton;
        QProgressBar *progressBar;
        QComboBox *modeCombo;
        QComboBox *mapCombo;
        QComboBox *rankCombo;
        QComboBox *seasonCombo;
        QButtonGroup *sideGroup;
        QJsonObject cachedDocument;
        QPoint dragStartPosition;
        bool draggingWindow = false;
        bool loadingSelectionData = false;
        int sortColumn = 3;
        bool sortDescending = true;
        QHash<int, int> sortClickCounts;
        std::future<bool> refreshFuture;
};


#endif //MAINWINDOW_H
