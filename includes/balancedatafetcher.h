#ifndef BALANCEDATAFETCHER_H
#define BALANCEDATAFETCHER_H

#include <QJsonObject>
#include <QJsonArray>
#include <QList>
#include <QString>
#include <QUrl>
#include <functional>

class BalanceDataFetcher
{
public:
    using ProgressCallback = std::function<void(int progress, const QString &message)>;

    static QString defaultCacheDir();
    // Updates website-provided filter options only; existing match data is retained.
    static bool refreshData(const QString &cacheDir = QString(), const ProgressCallback &progressCallback = ProgressCallback());
    static bool isSelectionCached(const QStringList &modeCodes,
                                  const QString &mapCode,
                                  const QString &seasonCode,
                                  const QStringList &rankCodes,
                                  const QString &cacheDir = QString());
    static bool ensureSelectionData(const QStringList &modeCodes,
                                    const QString &mapCode,
                                    const QString &seasonCode,
                                    const QStringList &rankCodes,
                                    const QString &cacheDir = QString(),
                                    const ProgressCallback &progressCallback = ProgressCallback());
    static QJsonObject loadCachedData(const QString &cacheDir = QString());
    static QList<QJsonObject> displayRows(const QString &cacheDir = QString());
    static QString lastError();

private:
    static QString makeAbsolutePath(const QString &cacheDir, const QString &relativePath);
    static QString normalizeRelativePath(const QString &path);
    static bool ensureDirectory(const QString &directoryPath);
    static bool writeJsonFile(const QString &path, const QJsonObject &document);
    static QByteArray fetchHttp(const QString &method,
                                const QUrl &url,
                                 const QByteArray &payload = QByteArray(),
                                 const std::function<void()> &waitingCallback = {});
    static QJsonObject fetchPageConfiguration();
    static QJsonArray fetchRoleAvatarMetadata(const QJsonObject &config);
    static QJsonArray fetchSeasonAndRankRows(const QJsonObject &config,
                                             const QJsonArray &roleMetadata,
                                             const QStringList &modeCodes,
                                             const QString &mapCode,
                                             const QString &seasonCode,
                                             const QStringList &rankCodes,
                                             const ProgressCallback &progressCallback);
    static QJsonObject filterOptions(const QJsonObject &config);
    static QString selectionKey(const QString &modeCode, const QString &mapCode,
                                const QString &seasonCode, const QString &rankCode);
    static QString resolveRoleIconUrl(const QJsonObject &roleObject);
    static QString downloadRoleIcon(const QString &roleCode,
                                    const QString &roleName,
                                    const QString &iconUrl,
                                    const QString &cacheDir);
    static QString safeStringFromObject(const QJsonObject &object, const QStringList &keys, const QString &fallback = QString());
    static double safeDoubleFromObject(const QJsonObject &object, const QStringList &keys, double fallback = 0.0);
};

#endif // BALANCEDATAFETCHER_H
