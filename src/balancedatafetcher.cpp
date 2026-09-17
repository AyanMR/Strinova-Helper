#include "balancedatafetcher.h"

#include <QDateTime>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QMap>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <QTimer>
#include <algorithm>

namespace
{
constexpr auto kApiBaseUrl = "https://klbq-prod-www.idreamsky.com";
constexpr auto kChartId = "338985";
constexpr auto kIdeToken = "b7FM3m";
QString g_lastError;

QJsonObject asObject(const QJsonValue &value) { return value.isObject() ? value.toObject() : QJsonObject(); }
QJsonArray asArray(const QJsonValue &value) { return value.isArray() ? value.toArray() : QJsonArray(); }

QJsonObject parseContentObject(const QJsonValue &value)
{
    if (value.isObject()) return value.toObject();
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(value.toString().toUtf8(), &error);
    return error.error == QJsonParseError::NoError && document.isObject() ? document.object() : QJsonObject();
}

QJsonArray contentObjects(const QJsonArray &items)
{
    QJsonArray result;
    for (const QJsonValue &item : items)
    {
        const QJsonObject content = parseContentObject(asObject(item).value("content"));
        if (!content.isEmpty()) result.append(content);
    }
    return result;
}

QString stringValue(const QJsonObject &object, const QStringList &keys, const QString &fallback = QString())
{
    for (const QString &key : keys)
    {
        const QJsonValue value = object.value(key);
        if (!value.isNull() && !value.isUndefined())
        {
            const QString text = value.toVariant().toString();
            if (!text.isEmpty()) return text;
        }
    }
    return fallback;
}

bool isHtmlPayload(const QByteArray &payload)
{
    const QByteArray trimmed = payload.trimmed().toLower();
    return trimmed.startsWith("<!doctype") || trimmed.startsWith("<html") || trimmed.startsWith("<script");
}

QString safeFileStem(QString name)
{
    name.replace(QRegularExpression(R"([\\/:*?"<>|])"), "_");
    return name.isEmpty() ? QStringLiteral("unknown") : name;
}

QString imageExtension(const QString &url)
{
    const QString path = QUrl(url).path();
    const int dot = path.lastIndexOf('.');
    const QString extension = dot < 0 ? QString() : path.mid(dot + 1).toLower();
    return extension.isEmpty() || extension.size() > 6 ? QStringLiteral(".png") : "." + extension;
}

void reportProgress(const BalanceDataFetcher::ProgressCallback &callback, const int progress, const QString &message)
{
    if (callback) callback(progress, message);
}
} // namespace

QString BalanceDataFetcher::defaultCacheDir()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)).absoluteFilePath("balance_data");
}

QString BalanceDataFetcher::lastError() { return g_lastError; }
QString BalanceDataFetcher::makeAbsolutePath(const QString &cacheDir, const QString &relativePath) { return QDir(cacheDir).absoluteFilePath(relativePath); }
QString BalanceDataFetcher::normalizeRelativePath(const QString &path) { return QDir::cleanPath(path).replace('\\', '/'); }
bool BalanceDataFetcher::ensureDirectory(const QString &directoryPath) { return QDir().mkpath(directoryPath); }

bool BalanceDataFetcher::writeJsonFile(const QString &path, const QJsonObject &document)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(QJsonDocument(document).toJson(QJsonDocument::Indented)) < 0 || !file.commit())
    {
        g_lastError = QString("无法写入缓存文件：%1").arg(path);
        return false;
    }
    return true;
}

QByteArray BalanceDataFetcher::fetchHttp(const QString &method, const QUrl &url, const QByteArray &payload,
                                         const std::function<void()> &waitingCallback)
{
    QNetworkAccessManager manager;
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 Chrome/126 Safari/537.36"));
    request.setRawHeader("Accept", "application/json, text/plain, */*");
    request.setRawHeader("Referer", "https://klbq.idreamsky.com/balanceData");
    request.setRawHeader("Origin", "https://klbq.idreamsky.com");
    QNetworkReply *reply = nullptr;
    if (method.compare("POST", Qt::CaseInsensitive) == 0)
    {
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        reply = manager.post(request, payload);
    }
    else reply = manager.get(request);
    QEventLoop loop;
    bool timedOut = false;
    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);
    QTimer waitingTimer;
    waitingTimer.setInterval(1000);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timeoutTimer, &QTimer::timeout, reply, [&] {
        timedOut = true;
        reply->abort();
    });
    if (waitingCallback)
    {
        QObject::connect(&waitingTimer, &QTimer::timeout, &loop, waitingCallback);
        waitingTimer.start();
    }
    timeoutTimer.start(12000);
    loop.exec();
    timeoutTimer.stop();
    waitingTimer.stop();
    QByteArray result;
    if (timedOut) g_lastError = QString("网络请求超时（%1）").arg(url.toString());
    else if (reply->error() == QNetworkReply::NoError) result = reply->readAll();
    else g_lastError = QString("网络请求失败（%1）：%2").arg(url.toString(), reply->errorString());
    reply->deleteLater();
    return result;
}

QJsonObject BalanceDataFetcher::fetchPageConfiguration()
{
    const QByteArray response = fetchHttp("GET", QUrl(QString::fromLatin1(kApiBaseUrl) + "/api/pages/KLBQ_BALANCE/index"));
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(response, &error);
    const QJsonObject config = asObject(asObject(document.object().value("data")).value("value"));
    if (response.isEmpty() || isHtmlPayload(response) || error.error != QJsonParseError::NoError || config.isEmpty())
    {
        if (g_lastError.isEmpty()) g_lastError = QStringLiteral("未能获取角色数据页面配置。");
        return QJsonObject();
    }
    return config;
}

QJsonArray BalanceDataFetcher::fetchRoleAvatarMetadata(const QJsonObject &config)
{
    QJsonArray avatars;
    const QJsonObject roleList = asObject(config.value("role_list"));
    for (const QJsonValue &roleValue : contentObjects(asArray(roleList.value("role_list"))))
    {
        const QJsonObject role = asObject(roleValue);
        const QString code = stringValue(role, {"character_code", "characterCode", "id", "code"});
        if (code.isEmpty()) continue;
        avatars.append(QJsonObject{{"code", code}, {"name", stringValue(role, {"character_name", "characterName", "name"}, code)},
                                  {"iconUrl", resolveRoleIconUrl(role)}});
    }
    return avatars;
}

QJsonObject BalanceDataFetcher::filterOptions(const QJsonObject &config)
{
    const QJsonObject settings = asObject(config.value("setting"));
    const auto makeOptions = [&settings](const QString &key) {
        QJsonArray result;
        for (const QJsonValue &value : contentObjects(asArray(settings.value(key))))
        {
            const QJsonObject option = value.toObject();
            const QString code = option.value("code").toString();
            const QString name = option.value("name").toString();
            if (!code.isEmpty() && !name.isEmpty()) result.append(QJsonObject{{"code", code}, {"name", name}});
        }
        return result;
    };
    return QJsonObject{{"modes", makeOptions("mode")}, {"maps", makeOptions("map")},
                       {"seasons", makeOptions("season")}, {"ranks", makeOptions("rank")}};
}

QString rankSelectionCode(QStringList rankCodes)
{
    std::sort(rankCodes.begin(), rankCodes.end());
    return rankCodes.join(QChar(0x1f));
}

QJsonArray BalanceDataFetcher::fetchSeasonAndRankRows(const QJsonObject &config, const QJsonArray &roleMetadata,
                                                      const QStringList &modeCodes, const QString &mapCode,
                                                      const QString &seasonCode, const QStringList &rankCodes,
                                                      const ProgressCallback &progressCallback)
{
    const QJsonObject settings = asObject(config.value("setting"));
    const QJsonArray modes = contentObjects(asArray(settings.value("mode")));
    const QJsonArray maps = contentObjects(asArray(settings.value("map")));
    const QJsonArray seasons = contentObjects(asArray(settings.value("season")));
    const QJsonArray ranks = contentObjects(asArray(settings.value("rank")));
    const auto matchingOptions = [](const QJsonArray &options, const QStringList &codes) {
        QJsonArray result;
        for (const QJsonValue &value : options)
            if (codes.contains(value.toObject().value("code").toString())) result.append(value);
        return result;
    };
    const QJsonArray selectedModes = matchingOptions(modes, modeCodes);
    const QJsonArray selectedMaps = matchingOptions(maps, QStringList{mapCode});
    const QJsonArray selectedSeasons = matchingOptions(seasons, QStringList{seasonCode});
    const QJsonArray selectedRanks = matchingOptions(ranks, rankCodes);
    if (selectedModes.isEmpty() || selectedMaps.isEmpty() || selectedSeasons.isEmpty() || selectedRanks.isEmpty())
    {
        g_lastError = QStringLiteral("页面配置中缺少模式、地图、赛季或段位。");
        return QJsonArray();
    }

    QMap<QString, QJsonObject> roles;
    for (const QJsonValue &value : roleMetadata) roles.insert(value.toObject().value("code").toString(), value.toObject());
    QJsonArray rows;
    QStringList selectedRankCodes;
    QStringList selectedRankNames;
    for (const QJsonValue &value : selectedRanks)
    {
        const QJsonObject rank = value.toObject();
        selectedRankCodes.append(rank.value("code").toString());
        selectedRankNames.append(rank.value("name").toString());
    }
    const QString rankSelectionCode = ::rankSelectionCode(selectedRankCodes);
    const QString rankSelectionName = selectedRankNames.join(QStringLiteral(" + "));
    const qsizetype totalRequests = selectedModes.size() * selectedMaps.size() * selectedSeasons.size();
    qsizetype completedRequests = 0;
    for (const QJsonValue &modeValue : selectedModes)
    for (const QJsonValue &mapValue : selectedMaps)
    for (const QJsonValue &seasonValue : selectedSeasons)
    {
        const QJsonObject mode = modeValue.toObject(), map = mapValue.toObject(), season = seasonValue.toObject();
        const QString modeCode = mode.value("code").toString(), mapCode = map.value("code").toString();
        const QString seasonCode = season.value("code").toString();
        const QJsonObject payload{{"iChartId", kChartId}, {"iSubChartId", kChartId}, {"sIdeToken", kIdeToken}, {"mode", modeCode},
                                  {"map", mapCode}, {"rank", QJsonArray::fromStringList(selectedRankCodes)}, {"season1", seasonCode}, {"season2", "0"}};
        const QString requestName = QStringLiteral("%1 / %2 / %3 / %4")
                                        .arg(mode.value("name").toString(), map.value("name").toString(),
                                             season.value("name").toString(), rankSelectionName);
        const int requestProgress = 55 + static_cast<int>(completedRequests * 31 / qMax<qsizetype>(totalRequests, 1));
        reportProgress(progressCallback, requestProgress,
                       QStringLiteral("正在获取赛季与段位数据（%1/%2）：%3")
                           .arg(completedRequests + 1).arg(totalRequests).arg(requestName));
        const QByteArray response = fetchHttp("POST", QUrl(QString::fromLatin1(kApiBaseUrl) + "/api/common/ide"),
                                             QJsonDocument(payload).toJson(QJsonDocument::Compact),
                                             [progressCallback, requestProgress, completedRequests, totalRequests, requestName] {
                                                 reportProgress(progressCallback, requestProgress,
                                                                QStringLiteral("正在等待服务器响应（%1/%2）：%3")
                                                                    .arg(completedRequests + 1).arg(totalRequests).arg(requestName));
                                             });
        ++completedRequests;
        QJsonParseError error;
        const QJsonDocument document = QJsonDocument::fromJson(response, &error);
        const QJsonObject root = document.object();
        const QJsonObject responseData = asObject(root.value("data"));
        const QJsonObject ideResponse = root.contains("iRet") ? root : responseData;
        const QJsonObject jData = parseContentObject(ideResponse.value("jData"));
        const QJsonObject data1 = asObject(jData.value("data1"));
        if (response.isEmpty() || error.error != QJsonParseError::NoError || ideResponse.value("iRet").toInt(-1) != 0)
        {
            if (g_lastError.isEmpty()) g_lastError = QString("获取数据失败：%1 / %2 / %3 / %4").arg(mode.value("name").toString(), map.value("name").toString(), season.value("name").toString(), rankSelectionName);
            return QJsonArray();
        }
        if (data1.isEmpty()) continue;
        const QList<QPair<QString, QJsonArray>> sides{{QStringLiteral("进攻方"), asArray(data1.value("side1"))}, {QStringLiteral("防守方"), asArray(data1.value("side2"))}};
        for (const auto &[sideName, entries] : sides)
        for (const QJsonValue &entryValue : entries)
        {
            const QJsonObject entry = entryValue.toObject();
            const QString heroId = stringValue(entry, {"id", "character_code", "characterCode"});
            const QJsonObject role = roles.value(heroId);
            QJsonObject row;
            row["mode"] = mode.value("name").toString();
            row["modeCode"] = modeCode;
            row["map"] = map.value("name").toString();
            row["mapCode"] = mapCode;
            row["season"] = season.value("name").toString();
            row["seasonCode"] = seasonCode;
            row["rank"] = rankSelectionName;
            row["rankCode"] = rankSelectionCode;
            row["side"] = sideName;
            row["heroId"] = heroId;
            row["heroName"] = stringValue(role, {"name"}, stringValue(entry, {"heroName", "name"}, QStringLiteral("未知角色")));
            row["iconPath"] = role.value("iconPath").toString();
            row["winRate"] = safeDoubleFromObject(entry, {"winRate", "win_rate"});
            row["pickRate"] = safeDoubleFromObject(entry, {"selectRate", "pickRate", "useRate", "select_rate"});
            row["kd"] = safeDoubleFromObject(entry, {"kd", "kD"});
            row["damageAve"] = safeDoubleFromObject(entry, {"damageAve", "damage_ave"});
            row["score"] = safeDoubleFromObject(entry, {"score"});
            rows.append(row);
        }
    }
    reportProgress(progressCallback, 86, QStringLiteral("赛季与段位数据获取完成，正在汇总结果…"));
    return rows;
}

QString BalanceDataFetcher::resolveRoleIconUrl(const QJsonObject &roleObject) { return stringValue(roleObject, {"character_image", "icon_url", "iconUrl", "icon", "img_url", "imgUrl", "avatar_url", "avatarUrl"}); }

QString BalanceDataFetcher::downloadRoleIcon(const QString &roleCode, const QString &roleName, const QString &iconUrl, const QString &cacheDir)
{
    if (iconUrl.isEmpty()) return QString();
    const QString avatarDir = makeAbsolutePath(cacheDir, "avatars");
    if (!ensureDirectory(avatarDir)) return QString();
    const QString targetFile = QDir(avatarDir).absoluteFilePath(safeFileStem(roleCode.isEmpty() ? roleName : roleCode) + imageExtension(iconUrl));
    if (QFile::exists(targetFile)) return normalizeRelativePath(QDir(cacheDir).relativeFilePath(targetFile));
    const QByteArray image = fetchHttp("GET", QUrl(iconUrl));
    if (image.isEmpty() || isHtmlPayload(image)) return QString();
    QSaveFile output(targetFile);
    if (!output.open(QIODevice::WriteOnly) || output.write(image) < 0 || !output.commit()) return QString();
    return normalizeRelativePath(QDir(cacheDir).relativeFilePath(targetFile));
}

QString BalanceDataFetcher::safeStringFromObject(const QJsonObject &object, const QStringList &keys, const QString &fallback) { return stringValue(object, keys, fallback); }
double BalanceDataFetcher::safeDoubleFromObject(const QJsonObject &object, const QStringList &keys, double fallback)
{
    for (const QString &key : keys)
    {
        const QJsonValue value = object.value(key);
        if (!value.isUndefined() && !value.isNull()) return value.toDouble(fallback);
    }
    return fallback;
}

bool BalanceDataFetcher::refreshData(const QString &cacheDir, const ProgressCallback &progressCallback)
{
    g_lastError.clear();
    const QString optionCacheDir = cacheDir.isEmpty() ? defaultCacheDir() : cacheDir;
    if (!ensureDirectory(optionCacheDir) || !ensureDirectory(makeAbsolutePath(optionCacheDir, "avatars")))
    {
        g_lastError = QStringLiteral("Unable to create the data cache directory.");
        return false;
    }
    reportProgress(progressCallback, 10, QStringLiteral("Checking website filter options..."));
    const QJsonObject optionConfig = fetchPageConfiguration();
    if (optionConfig.isEmpty()) return false;
    const QJsonObject optionDocument = filterOptions(optionConfig);
    QJsonObject cachedDocument = loadCachedData(optionCacheDir);
    const bool optionsCurrent = cachedDocument.value("filterOptions").toObject() == optionDocument;
    const bool requiresSideMappingRefresh = cachedDocument.value("sideMappingVersion").toInt() != 3;
    QJsonArray avatars = optionsCurrent ? cachedDocument.value("avatars").toArray() : fetchRoleAvatarMetadata(optionConfig);
    bool missingAvatarPath = avatars.isEmpty();
    for (const QJsonValue &value : avatars)
        if (value.toObject().value("iconPath").toString().isEmpty()) missingAvatarPath = true;
    if (optionsCurrent && !missingAvatarPath && !requiresSideMappingRefresh)
    {
        reportProgress(progressCallback, 100, QStringLiteral("Filter options are already current."));
        return true;
    }
    if (avatars.isEmpty()) avatars = fetchRoleAvatarMetadata(optionConfig);
    reportProgress(progressCallback, 40, QStringLiteral("Caching role icons..."));
    for (qsizetype index = 0; index < avatars.size(); ++index)
    {
        QJsonObject avatar = avatars.at(index).toObject();
        avatar["iconPath"] = downloadRoleIcon(avatar.value("code").toString(), avatar.value("name").toString(),
                                               avatar.value("iconUrl").toString(), optionCacheDir);
        avatars[index] = avatar;
        reportProgress(progressCallback, 40 + static_cast<int>((index + 1) * 50 / qMax<qsizetype>(avatars.size(), 1)),
                       QStringLiteral("Caching role icons..."));
    }
    cachedDocument["settings"] = optionConfig;
    cachedDocument["filterOptions"] = optionDocument;
    cachedDocument["avatars"] = avatars;
    cachedDocument["sideMappingVersion"] = 3;
    if (requiresSideMappingRefresh)
    {
        cachedDocument["rows"] = QJsonArray();
        cachedDocument["cachedSelections"] = QJsonArray();
    }
    if (!cachedDocument.contains("rows")) cachedDocument["rows"] = QJsonArray();
    if (!cachedDocument.contains("cachedSelections")) cachedDocument["cachedSelections"] = QJsonArray();
    cachedDocument["updatedAt"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    const bool optionSaveSuccess = writeJsonFile(makeAbsolutePath(optionCacheDir, "cache.json"), cachedDocument);
    if (optionSaveSuccess) reportProgress(progressCallback, 100, optionsCurrent ? QStringLiteral("Role icons cached.") : QStringLiteral("Filter options updated."));
    return optionSaveSuccess;

}

QString BalanceDataFetcher::selectionKey(const QString &modeCode, const QString &mapCode,
                                         const QString &seasonCode, const QString &rankCode)
{
    return modeCode + QChar(0x1f) + mapCode + QChar(0x1f) + seasonCode + QChar(0x1f) + rankCode;
}

bool BalanceDataFetcher::isSelectionCached(const QStringList &modeCodes, const QString &mapCode,
                                           const QString &seasonCode, const QStringList &rankCodes,
                                           const QString &cacheDir)
{
    if (modeCodes.isEmpty() || mapCode.isEmpty() || seasonCode.isEmpty() || rankCodes.isEmpty()) return false;
    const QJsonObject cache = loadCachedData(cacheDir);
    QSet<QString> cached;
    for (const QJsonValue &value : cache.value("cachedSelections").toArray()) cached.insert(value.toString());
    const QString selectedRanks = rankSelectionCode(rankCodes);
    for (const QString &modeCode : modeCodes)
        if (!cached.contains(selectionKey(modeCode, mapCode, seasonCode, selectedRanks))) return false;
    return true;
}

bool BalanceDataFetcher::ensureSelectionData(const QStringList &modeCodes, const QString &mapCode,
                                             const QString &seasonCode, const QStringList &rankCodes,
                                             const QString &cacheDir, const ProgressCallback &progressCallback)
{
    g_lastError.clear();
    if (modeCodes.isEmpty() || mapCode.isEmpty() || seasonCode.isEmpty() || rankCodes.isEmpty()) return true;
    const QString targetDir = cacheDir.isEmpty() ? defaultCacheDir() : cacheDir;
    QJsonObject document = loadCachedData(targetDir);
    const QJsonObject config = document.value("settings").toObject();
    if (config.isEmpty())
    {
        g_lastError = QStringLiteral("No website options are cached yet.");
        return false;
    }

    QSet<QString> cached;
    for (const QJsonValue &value : document.value("cachedSelections").toArray()) cached.insert(value.toString());
    QJsonArray rows = document.value("rows").toArray();
    const QJsonArray avatars = document.value("avatars").toArray();
    const QString selectedRanks = rankSelectionCode(rankCodes);
    QStringList missingModes;
    for (const QString &modeCode : modeCodes)
        if (!cached.contains(selectionKey(modeCode, mapCode, seasonCode, selectedRanks))) missingModes.append(modeCode);
    if (missingModes.isEmpty()) return true;

    for (qsizetype i = 0; i < missingModes.size(); ++i)
    {
        const QString &modeCode = missingModes.at(i);
        reportProgress(progressCallback, static_cast<int>(i * 90 / missingModes.size()),
                       QStringLiteral("Downloading selected data (%1/%2)...").arg(i + 1).arg(missingModes.size()));
        const QJsonArray downloaded = fetchSeasonAndRankRows(config, avatars, QStringList{modeCode}, mapCode,
                                                              seasonCode, rankCodes, progressCallback);
        if (!g_lastError.isEmpty()) return false;
        for (const QJsonValue &row : downloaded) rows.append(row);
        cached.insert(selectionKey(modeCode, mapCode, seasonCode, selectedRanks));
    }
    QJsonArray cachedSelections;
    for (const QString &key : cached) cachedSelections.append(key);
    document["rows"] = rows;
    document["cachedSelections"] = cachedSelections;
    document["updatedAt"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    reportProgress(progressCallback, 95, QStringLiteral("Saving selected data..."));
    const bool success = writeJsonFile(makeAbsolutePath(targetDir, "cache.json"), document);
    if (success) reportProgress(progressCallback, 100, QStringLiteral("Selected data cached."));
    return success;
}

QJsonObject BalanceDataFetcher::loadCachedData(const QString &cacheDir)
{
    QFile file(makeAbsolutePath(cacheDir.isEmpty() ? defaultCacheDir() : cacheDir, "cache.json"));
    if (!file.open(QIODevice::ReadOnly)) return QJsonObject();
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
    return error.error == QJsonParseError::NoError && document.isObject() ? document.object() : QJsonObject();
}

QList<QJsonObject> BalanceDataFetcher::displayRows(const QString &cacheDir)
{
    QList<QJsonObject> result;
    for (const QJsonValue &value : loadCachedData(cacheDir).value("rows").toArray()) if (value.isObject()) result.append(value.toObject());
    return result;
}
