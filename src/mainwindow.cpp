#include "mainwindow.h"
#include "balancedatafetcher.h"

#include <QAbstractButton>
#include <QAbstractItemView>
#include <QDir>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QHeaderView>
#include <QHash>
#include <QJsonArray>
#include <QLineEdit>
#include <QMetaObject>
#include <QMouseEvent>
#include <QPixmap>
#include <QPointer>
#include <QSet>
#include <QSignalBlocker>
#include <QStandardItemModel>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>
#include <chrono>

namespace
{
QLabel *createFieldLabel(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setObjectName("fieldLabel");
    return label;
}

QComboBox *createFilterCombo(QWidget *parent)
{
    auto *combo = new QComboBox(parent);
    combo->setObjectName("filterCombo");
    combo->setMinimumWidth(150);
    combo->addItem("全部");
    return combo;
}

void setComboOptions(QComboBox *combo, const QSet<QString> &values)
{
    const QString selected = combo->currentText();
    QStringList options = values.values();
    std::sort(options.begin(), options.end(), [](const QString &left, const QString &right) {
        return QString::localeAwareCompare(left, right) < 0;
    });

    const QSignalBlocker blocker(combo);
    combo->clear();
    combo->addItem("全部");
    for (const QString &value : options)
        if (!value.trimmed().isEmpty()) combo->addItem(value);
    combo->setCurrentText(combo->findText(selected) >= 0 ? selected : QStringLiteral("全部"));
}
const QString kAll = QString::fromUtf8("\xE5\x85\xA8\xE9\x83\xA8");
const QString kNone = QString::fromUtf8("\xE6\x97\xA0");

QString rankSelectionId(QStringList rankCodes)
{
    std::sort(rankCodes.begin(), rankCodes.end());
    return rankCodes.join(QChar(0x1f));
}

void setWebsiteOptions(QComboBox *combo, const QJsonArray &options, const QString &emptyLabel,
                       const QString &defaultOption = QString())
{
    const QString selectedCode = combo->currentData(Qt::UserRole).toString();
    const QSignalBlocker blocker(combo);
    combo->clear();
    if (!emptyLabel.isEmpty()) combo->addItem(emptyLabel, QString());
    for (const QJsonValue &value : options)
    {
        const QJsonObject option = value.toObject();
        const QString code = option.value("code").toString();
        const QString name = option.value("name").toString();
        if (!code.isEmpty() && !name.isEmpty()) combo->addItem(name, code);
    }
    const int selectedIndex = combo->findData(selectedCode, Qt::UserRole);
    const int defaultIndex = defaultOption.isEmpty() ? -1 : combo->findText(defaultOption, Qt::MatchExactly);
    combo->setCurrentIndex(selectedIndex >= 0 ? selectedIndex : (defaultIndex >= 0 ? defaultIndex : 0));
}
} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QWidget(parent), titleBar(nullptr), dataTree(nullptr), statusLabel(nullptr), refreshButton(nullptr), progressBar(nullptr),
      modeCombo(nullptr), mapCombo(nullptr), rankCombo(nullptr), seasonCombo(nullptr), sideGroup(nullptr)
{
    setWindowTitle("卡丘助手 · 数据查询");
    setWindowFlags(Qt::FramelessWindowHint | Qt::Window);
    setAttribute(Qt::WA_TranslucentBackground);
    setMinimumSize(860, 780);
    resize(850, 980);

    auto *windowLayout = new QVBoxLayout(this);
    windowLayout->setContentsMargins(16, 16, 16, 16);

    auto *panel = new QFrame(this);
    panel->setObjectName("glassPanel");
    auto *shadow = new QGraphicsDropShadowEffect(panel);
    shadow->setBlurRadius(35);
    shadow->setOffset(0, 12);
    shadow->setColor(QColor(48, 67, 91, 70));
    panel->setGraphicsEffect(shadow);
    windowLayout->addWidget(panel);

    auto *panelLayout = new QVBoxLayout(panel);
    panelLayout->setContentsMargins(28, 18, 28, 26);
    panelLayout->setSpacing(16);

    titleBar = new QWidget(panel);
    titleBar->setObjectName("titleBar");
    titleBar->setCursor(Qt::SizeAllCursor);
    titleBar->installEventFilter(this);
    auto *titleLayout = new QHBoxLayout(titleBar);
    titleLayout->setContentsMargins(0, 0, 0, 0);
    auto *titleBlock = new QVBoxLayout();
    titleBlock->setSpacing(1);
    auto *title = new QLabel("数据查询", titleBar);
    title->setObjectName("windowTitle");
    title->setAttribute(Qt::WA_TransparentForMouseEvents);
    auto *subtitle = new QLabel("Strinova · 对局平衡数据", titleBar);
    subtitle->setObjectName("windowSubtitle");
    subtitle->setAttribute(Qt::WA_TransparentForMouseEvents);
    titleBlock->addWidget(title);
    titleBlock->addWidget(subtitle);
    titleLayout->addLayout(titleBlock);
    titleLayout->addStretch();
    auto *minimizeButton = new QPushButton("−", titleBar);
    minimizeButton->setObjectName("windowControl");
    minimizeButton->setToolTip("最小化");
    auto *closeButton = new QPushButton("×", titleBar);
    closeButton->setObjectName("closeControl");
    closeButton->setToolTip("隐藏窗口");
    titleLayout->addWidget(minimizeButton);
    titleLayout->addWidget(closeButton);
    connect(minimizeButton, &QPushButton::clicked, this, &QWidget::showMinimized);
    connect(closeButton, &QPushButton::clicked, this, &QWidget::hide);
    panelLayout->addWidget(titleBar);

    auto *filterCard = new QFrame(panel);
    filterCard->setObjectName("filterCard");
    auto *filterLayout = new QVBoxLayout(filterCard);
    filterLayout->setContentsMargins(20, 17, 20, 17);
    filterLayout->setSpacing(12);
    auto *filterHeader = new QHBoxLayout();
    auto *filterTitle = new QLabel("筛选条件", filterCard);
    filterTitle->setObjectName("sectionTitle");
    auto *filterHint = new QLabel("选择组合后自动更新结果", filterCard);
    filterHint->setObjectName("sectionHint");
    filterHeader->addWidget(filterTitle);
    filterHeader->addStretch();
    filterHeader->addWidget(filterHint);
    filterLayout->addLayout(filterHeader);

    auto *filters = new QHBoxLayout();
    filters->setSpacing(14);
    modeCombo = createFilterCombo(filterCard);
    mapCombo = createFilterCombo(filterCard);
    rankCombo = createFilterCombo(filterCard);
    seasonCombo = createFilterCombo(filterCard);
    rankCombo->setEditable(true);
    rankCombo->lineEdit()->setReadOnly(true);
    rankCombo->lineEdit()->setPlaceholderText(kNone);
    rankCombo->installEventFilter(this);
    rankCombo->lineEdit()->installEventFilter(this);
    rankCombo->view()->viewport()->installEventFilter(this);
    const QList<QPair<QString, QComboBox *>> filterPairs{{"游戏模式", modeCombo}, {"地图", mapCombo}, {"段位", rankCombo}, {"赛季", seasonCombo}};
    for (const auto &[labelText, combo] : filterPairs)
    {
        auto *field = new QVBoxLayout();
        field->setSpacing(6);
        field->addWidget(createFieldLabel(labelText, filterCard));
        field->addWidget(combo);
        filters->addLayout(field, 1);
    }
    filterLayout->addLayout(filters);

    auto *sideRow = new QHBoxLayout();
    sideRow->setSpacing(8);
    sideRow->addWidget(createFieldLabel("阵营", filterCard));
    sideGroup = new QButtonGroup(this);
    const QList<QPair<QString, QString>> sides{{"进攻方", "进攻方"}, {"防守方", "防守方"}};
    for (const auto &[text, value] : sides)
    {
        auto *button = new QPushButton(text, filterCard);
        button->setObjectName("sideButton");
        button->setCheckable(true);
        button->setProperty("sideValue", value);
        sideGroup->addButton(button);
        sideRow->addWidget(button);
        if (value == "防守方") button->setChecked(true);
    }
    sideRow->addStretch();
    filterLayout->addLayout(sideRow);
    panelLayout->addWidget(filterCard);

    auto *dataHeader = new QHBoxLayout();
    auto *dataTitle = new QLabel("平衡数据", panel);
    dataTitle->setObjectName("sectionTitle");
    statusLabel = new QLabel("正在准备数据…", panel);
    statusLabel->setObjectName("statusLabel");
    refreshButton = new QPushButton("获取最新数据", panel);
    refreshButton->setObjectName("refreshButton");
    dataHeader->addWidget(dataTitle);
    dataHeader->addStretch();
    dataHeader->addWidget(statusLabel);
    dataHeader->addWidget(refreshButton);
    panelLayout->addLayout(dataHeader);

    progressBar = new QProgressBar(panel);
    progressBar->setObjectName("refreshProgress");
    progressBar->setRange(0, 100);
    progressBar->setValue(0);
    progressBar->setTextVisible(false);
    panelLayout->addWidget(progressBar);

    dataTree = new QTreeWidget(panel);
    dataTree->setObjectName("dataTree");
    dataTree->setColumnCount(7);
    dataTree->setHeaderLabels({QString(), QString(), "选取率", "胜率", "K/D", "回合均伤", "输出分"});
    dataTree->setAlternatingRowColors(false);
    dataTree->setRootIsDecorated(false);
    dataTree->setUniformRowHeights(true);
    dataTree->setIconSize(QSize(36, 36));
    dataTree->setIndentation(0);
    dataTree->header()->setStretchLastSection(true);
    dataTree->header()->setSectionsClickable(true);
    dataTree->header()->setSortIndicatorShown(true);
    dataTree->header()->setSortIndicator(sortColumn, Qt::DescendingOrder);
    dataTree->header()->setSectionResizeMode(0, QHeaderView::Fixed);
    dataTree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    dataTree->setColumnWidth(0, 52);
    panelLayout->addWidget(dataTree, 1);

    setStyleSheet(R"(
        QWidget { color: #203047; font-size: 13px; }
        QFrame#glassPanel { background: rgba(250, 253, 255, 224); border: 1px solid rgba(255, 255, 255, 230); border-radius: 22px; }
        QWidget#titleBar { min-height: 46px; }
        QLabel#windowTitle { color: #17243b; font-size: 23px; font-weight: 700; }
        QLabel#windowSubtitle { color: #8290a3; font-size: 12px; }
        QPushButton#windowControl, QPushButton#closeControl { background: rgba(226, 235, 246, 180); border: 1px solid rgba(214, 225, 239, 200); border-radius: 14px; color: #5f7188; font-size: 20px; font-weight: 500; min-width: 28px; max-width: 28px; min-height: 28px; max-height: 28px; padding: 0; }
        QPushButton#windowControl:hover { background: rgba(207, 222, 240, 220); color: #31445e; }
        QPushButton#closeControl:hover { background: #ef6a6a; border-color: #ef6a6a; color: white; }
        QFrame#filterCard { background: rgba(255, 255, 255, 154); border: 1px solid rgba(216, 228, 242, 205); border-radius: 16px; }
        QLabel#sectionTitle { color: #273b57; font-size: 16px; font-weight: 700; }
        QLabel#sectionHint, QLabel#statusLabel { color: #79889b; font-size: 12px; }
        QLabel#fieldLabel { color: #718199; font-size: 12px; font-weight: 600; }
        QComboBox#filterCombo { background: rgba(245, 249, 254, 220); border: 1px solid #d7e2f0; border-radius: 9px; color: #273b57; min-height: 32px; padding: 0 28px 0 10px; }
        QComboBox#filterCombo:hover, QComboBox#filterCombo:focus { border: 1px solid #82aeee; background: white; }
        QComboBox#filterCombo::drop-down { border: none; width: 25px; }
        QComboBox#filterCombo::down-arrow { image: none; border: solid #6682a6; border-width: 0 1px 1px 0; width: 6px; height: 6px; }
        QComboBox QAbstractItemView { background: #ffffff; border: 1px solid #d7e2f0; border-radius: 8px; padding: 5px; selection-background-color: #e3efff; selection-color: #24579e; }
        QPushButton#sideButton { background: rgba(242, 247, 253, 190); border: 1px solid #d9e4f1; border-radius: 9px; color: #61738b; min-width: 72px; min-height: 31px; padding: 0 10px; }
        QPushButton#sideButton:hover { background: #edf5ff; border-color: #9fc2f2; }
        QPushButton#sideButton:checked { background: #397dda; border-color: #397dda; color: white; font-weight: 700; }
        QPushButton#refreshButton { background: #397dda; border: none; border-radius: 9px; color: white; font-weight: 700; min-height: 34px; padding: 0 15px; }
        QPushButton#refreshButton:hover { background: #286ecb; }
        QPushButton#refreshButton:disabled { background: #a9bdd7; }
        QProgressBar#refreshProgress { background: rgba(219, 230, 243, 165); border: none; border-radius: 4px; min-height: 7px; max-height: 7px; }
        QProgressBar#refreshProgress::chunk { background: #4e91eb; border-radius: 4px; }
        QTreeWidget#dataTree { background: rgba(255, 255, 255, 128); border: 1px solid rgba(213, 225, 239, 210); border-radius: 14px; outline: none; padding: 5px; }
        QTreeWidget#dataTree::item { min-height: 48px; border-bottom: 1px solid rgba(220, 229, 240, 150); padding: 4px 6px; }
        QTreeWidget#dataTree::item:hover { background: rgba(226, 239, 255, 185); }
        QTreeWidget#dataTree::item:selected { background: rgba(205, 226, 252, 220); color: #1c4f90; }
        QHeaderView::section { background: transparent; border: none; border-bottom: 1px solid #d9e5f1; color: #8290a3; font-size: 12px; font-weight: 700; height: 34px; padding: 0 8px; }
    )");

    connect(refreshButton, &QPushButton::clicked, this, &MainWindow::refreshBalanceData);
    connect(dataTree->header(), &QHeaderView::sectionClicked, this, [this](const int column) {
        if (column < 2 || column > 6) return;
        sortColumn = column;
        sortDescending = (++sortClickCounts[column] % 2) == 1;
        dataTree->header()->setSortIndicator(column, sortDescending ? Qt::DescendingOrder : Qt::AscendingOrder);
        applyFilters();
    });
    for (QComboBox *combo : {modeCombo, mapCombo, seasonCombo})
        connect(combo, &QComboBox::currentIndexChanged, this, [this](int) {
            applyFilters();
            ensureCurrentSelectionData();
        });
    connect(sideGroup, &QButtonGroup::buttonClicked, this, [this](QAbstractButton *) { applyFilters(); });

    QTimer::singleShot(0, this, [this] {
        const QJsonObject cachedData = BalanceDataFetcher::loadCachedData();
        bool missingIcon = false;
        for (const QJsonValue &value : cachedData.value("avatars").toArray())
            if (value.toObject().value("iconPath").toString().isEmpty()) missingIcon = true;
        if (cachedData.isEmpty() || cachedData.value("filterOptions").toObject().isEmpty() || missingIcon || cachedData.value("sideMappingVersion").toInt() != 3) refreshBalanceData();
        else
        {
            populateDataTree(cachedData);
            setRefreshProgress(100, QString("已加载本地缓存 · %1").arg(cachedData.value("updatedAt").toString()));
        }
    });
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (rankCombo && (watched == rankCombo || watched == rankCombo->lineEdit()) && event->type() == QEvent::MouseButtonPress)
    {
        const auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() == Qt::LeftButton)
        {
            rankCombo->showPopup();
            return true;
        }
    }
    if (rankCombo && watched == rankCombo->view()->viewport())
    {
        if (event->type() == QEvent::MouseButtonPress)
        {
            const auto *mouseEvent = static_cast<QMouseEvent *>(event);
            if (mouseEvent->button() == Qt::LeftButton)
            {
                const QModelIndex index = rankCombo->view()->indexAt(mouseEvent->position().toPoint());
                if (index.isValid()) toggleRankOption(index.row());
                return true;
            }
        }
        if (event->type() == QEvent::MouseButtonRelease) return true;
    }
    if (watched != titleBar) return QWidget::eventFilter(watched, event);
    if (event->type() == QEvent::MouseButtonPress)
    {
        const auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() == Qt::LeftButton)
        {
            draggingWindow = true;
            dragStartPosition = mouseEvent->globalPosition().toPoint() - frameGeometry().topLeft();
            return true;
        }
    }
    else if (event->type() == QEvent::MouseMove && draggingWindow)
    {
        const auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->buttons() & Qt::LeftButton) move(mouseEvent->globalPosition().toPoint() - dragStartPosition);
        return true;
    }
    else if (event->type() == QEvent::MouseButtonRelease)
    {
        draggingWindow = false;
        return true;
    }
    return QWidget::eventFilter(watched, event);
}

QTreeWidgetItem *MainWindow::createDataItem(const QJsonObject &row, const QString &categoryLabel) const
{
    auto *item = new QTreeWidgetItem({QString(), row.value("heroName").toString(), QString::number(row.value("pickRate").toDouble(), 'f', 2) + "%", QString::number(row.value("winRate").toDouble(), 'f', 2) + "%", QString::number(row.value("kd").toDouble(), 'f', 2), QString::number(row.value("damageAve").toDouble(), 'f', 0), QString::number(row.value("score").toDouble(), 'f', 0)});
    item->setData(0, Qt::UserRole, categoryLabel);
    QString iconPath = row.value("iconPath").toString();
    if (iconPath.isEmpty())
        for (const QJsonValue &avatarValue : cachedDocument.value("avatars").toArray())
        {
            const QJsonObject avatar = avatarValue.toObject();
            if (avatar.value("code").toString() == row.value("heroId").toString())
            {
                iconPath = avatar.value("iconPath").toString();
                break;
            }
        }
    if (!iconPath.isEmpty())
    {
        const QPixmap pixmap(QDir(BalanceDataFetcher::defaultCacheDir()).absoluteFilePath(iconPath));
        if (!pixmap.isNull()) item->setIcon(0, QIcon(pixmap));
    }
    return item;
}

void MainWindow::populateFilterOptions(const QJsonObject &cacheDocument)
{
    const QJsonObject options = cacheDocument.value("filterOptions").toObject();
    setWebsiteOptions(modeCombo, options.value("modes").toArray(), QString(), QStringLiteral("排位爆破"));
    setWebsiteOptions(mapCombo, options.value("maps").toArray(), kNone);
    setWebsiteOptions(seasonCombo, options.value("seasons").toArray(), kNone);

    const QSignalBlocker blocker(rankCombo);
    const QStringList previouslySelected = selectedRankCodes();
    rankCombo->clear();
    for (const QJsonValue &value : options.value("ranks").toArray())
    {
        const QJsonObject option = value.toObject();
        const QString code = option.value("code").toString();
        const QString name = option.value("name").toString();
        if (code.isEmpty() || name.isEmpty()) continue;
        rankCombo->addItem(name, code);
        const int index = rankCombo->count() - 1;
        rankCombo->setItemData(index, previouslySelected.contains(code) ? Qt::Checked : Qt::Unchecked, Qt::CheckStateRole);
        if (auto *model = qobject_cast<QStandardItemModel *>(rankCombo->model()))
            if (auto *item = model->item(index)) item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
    }
    updateRankSummary();
}

void MainWindow::populateDataTree(const QJsonObject &cacheDocument)
{
    cachedDocument = cacheDocument;
    populateFilterOptions(cacheDocument);
    applyFilters();
}

void MainWindow::applyFilters()
{
    if (!dataTree) return;
    dataTree->clear();
    const QString selectedMode = modeCombo->currentText();
    const QString selectedMap = mapCombo->currentText();
    const QStringList selectedRankCodes = this->selectedRankCodes();
    const QString selectedRankCode = rankSelectionId(selectedRankCodes);
    const QString selectedSeason = seasonCombo->currentText();
    const QString selectedSide = sideGroup->checkedButton() ? sideGroup->checkedButton()->property("sideValue").toString() : QStringLiteral("全部");
    if (selectedMap == kNone || selectedSeason == kNone || selectedRankCodes.isEmpty())
    {
        statusLabel->setText(QStringLiteral("请选择地图、段位和赛季"));
        return;
    }
    const QJsonArray rows = cachedDocument.value("rows").toArray();
    QList<QJsonObject> visibleRows;
    for (const QJsonValue &value : rows)
    {
        const QJsonObject row = value.toObject();
        if ((selectedMode != kAll && row.value("mode").toString() != selectedMode) ||
            (selectedMap != kNone && row.value("map").toString() != selectedMap) ||
             (row.value("rankCode").toString() != selectedRankCode) ||
             (selectedSeason != kNone && row.value("season").toString() != selectedSeason) ||
             (selectedSide != kAll && row.value("side").toString() != selectedSide)) continue;
        visibleRows.append(row);
    }
    const auto metricValue = [](const QJsonObject &row, const int column) {
        switch (column)
        {
        case 2: return row.value("pickRate").toDouble();
        case 3: return row.value("winRate").toDouble();
        case 4: return row.value("kd").toDouble();
        case 5: return row.value("damageAve").toDouble();
        case 6: return row.value("score").toDouble();
        default: return 0.0;
        }
    };
    std::sort(visibleRows.begin(), visibleRows.end(), [this, &metricValue](const QJsonObject &left, const QJsonObject &right) {
        const double leftValue = metricValue(left, sortColumn);
        const double rightValue = metricValue(right, sortColumn);
        if (leftValue == rightValue) return QString::localeAwareCompare(left.value("heroName").toString(), right.value("heroName").toString()) < 0;
        return sortDescending ? leftValue > rightValue : leftValue < rightValue;
    });
    QHash<int, double> maximumValues;
    for (const QJsonObject &row : visibleRows)
        for (int column = 2; column <= 6; ++column)
            maximumValues[column] = qMax(maximumValues.value(column, metricValue(row, column)), metricValue(row, column));
    for (const QJsonObject &row : visibleRows)
    {
        auto *item = createDataItem(row, QString());
        for (int column = 2; column <= 6; ++column)
        {
            if (maximumValues.value(column) <= 0.0 || metricValue(row, column) != maximumValues.value(column)) continue;
            QFont leadingValueFont = item->font(column);
            leadingValueFont.setBold(true);
            leadingValueFont.setPointSizeF(qMax(leadingValueFont.pointSizeF(), 13.5));
            item->setFont(column, leadingValueFont);
            item->setForeground(column, QColor("#168b4b"));
        }
        dataTree->addTopLevelItem(item);
    }
    statusLabel->setText(!visibleRows.isEmpty() ? QString("显示 %1 / %2 条数据").arg(visibleRows.size()).arg(rows.size()) : QStringLiteral("没有符合当前筛选条件的数据"));
}

QStringList MainWindow::selectedRankCodes() const
{
    QStringList result;
    for (int index = 0; index < rankCombo->count(); ++index)
        if (rankCombo->itemData(index, Qt::CheckStateRole).toInt() == Qt::Checked)
            result.append(rankCombo->itemData(index, Qt::UserRole).toString());
    return result;
}

QStringList MainWindow::selectedModeCodes() const
{
    if (!modeCombo->currentData(Qt::UserRole).toString().isEmpty()) return {modeCombo->currentData(Qt::UserRole).toString()};
    QStringList result;
    for (const QJsonValue &value : cachedDocument.value("filterOptions").toObject().value("modes").toArray())
        result.append(value.toObject().value("code").toString());
    return result;
}

void MainWindow::toggleRankOption(const int index)
{
    if (index < 0 || index >= rankCombo->count()) return;
    const Qt::CheckState state = static_cast<Qt::CheckState>(rankCombo->itemData(index, Qt::CheckStateRole).toInt());
    rankCombo->setItemData(index, state == Qt::Checked ? Qt::Unchecked : Qt::Checked, Qt::CheckStateRole);
    updateRankSummary();
    applyFilters();
    ensureCurrentSelectionData();
}

void MainWindow::updateRankSummary()
{
    QStringList names;
    for (int index = 0; index < rankCombo->count(); ++index)
        if (rankCombo->itemData(index, Qt::CheckStateRole).toInt() == Qt::Checked) names.append(rankCombo->itemText(index));
    rankCombo->lineEdit()->setText(names.isEmpty() ? kNone : names.join(QStringLiteral(", ")));
}

void MainWindow::ensureCurrentSelectionData()
{
    if (refreshFuture.valid()) return;
    const QString mapCode = mapCombo->currentData(Qt::UserRole).toString();
    const QString seasonCode = seasonCombo->currentData(Qt::UserRole).toString();
    const QStringList modeCodes = selectedModeCodes();
    const QStringList rankCodes = selectedRankCodes();
    if (mapCode.isEmpty() || seasonCode.isEmpty() || modeCodes.isEmpty() || rankCodes.isEmpty()) return;
    if (BalanceDataFetcher::isSelectionCached(modeCodes, mapCode, seasonCode, rankCodes)) return;

    loadingSelectionData = true;
    refreshButton->setEnabled(false);
    setRefreshProgress(0, QStringLiteral("正在下载当前选择的数据..."));
    const QPointer<MainWindow> window(this);
    refreshFuture = std::async(std::launch::async, [window, modeCodes, mapCode, seasonCode, rankCodes] {
        return BalanceDataFetcher::ensureSelectionData(modeCodes, mapCode, seasonCode, rankCodes, QString(), [window](const int progress, const QString &message) {
            if (!window) return;
            QMetaObject::invokeMethod(window, [window, progress, message] {
                if (window) window->setRefreshProgress(progress, message);
            }, Qt::QueuedConnection);
        });
    });
    QTimer::singleShot(100, this, &MainWindow::finishBalanceDataRefresh);
}

void MainWindow::setRefreshProgress(const int progress, const QString &message)
{
    progressBar->setValue(progress);
    statusLabel->setText(message);
}

void MainWindow::refreshBalanceData()
{
    if (refreshFuture.valid()) return;
    loadingSelectionData = false;
    refreshButton->setEnabled(false);
    setRefreshProgress(0, QStringLiteral("准备获取最新数据…"));
    const QPointer<MainWindow> window(this);
    refreshFuture = std::async(std::launch::async, [window] {
        return BalanceDataFetcher::refreshData(QString(), [window](const int progress, const QString &message) {
            if (!window) return;
            QMetaObject::invokeMethod(window, [window, progress, message] {
                if (window) window->setRefreshProgress(progress, message);
            }, Qt::QueuedConnection);
        });
    });
    QTimer::singleShot(100, this, &MainWindow::finishBalanceDataRefresh);
}

void MainWindow::finishBalanceDataRefresh()
{
    if (refreshFuture.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready)
    {
        QTimer::singleShot(150, this, &MainWindow::finishBalanceDataRefresh);
        return;
    }
    const bool success = refreshFuture.get();
    refreshFuture = std::future<bool>();
    const bool completedSelectionLoad = loadingSelectionData;
    loadingSelectionData = false;
    refreshButton->setEnabled(true);
    if (success)
    {
        const QJsonObject cache = BalanceDataFetcher::loadCachedData();
        populateDataTree(cache);
        if (completedSelectionLoad)
        {
            setRefreshProgress(100, QString("已缓存当前选择 · 共 %1 条").arg(cache.value("rows").toArray().size()));
            ensureCurrentSelectionData();
            return;
        }
        setRefreshProgress(100, QString("网站选项已是最新 · 已缓存 %1 条数据").arg(cache.value("rows").toArray().size()));
        return;
    }
    const QJsonObject cached = BalanceDataFetcher::loadCachedData();
    if (!cached.isEmpty())
    {
        populateDataTree(cached);
        setRefreshProgress(0, QString("更新失败，已保留本地缓存：%1").arg(BalanceDataFetcher::lastError()));
    }
    else setRefreshProgress(0, QString("获取失败：%1").arg(BalanceDataFetcher::lastError()));
}
