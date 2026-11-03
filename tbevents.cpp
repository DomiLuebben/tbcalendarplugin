// SPDX-FileCopyrightText: 2026 tbcalendarplugin contributors
// SPDX-License-Identifier: GPL-3.0-only

#include "tbevents.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QTimeZone>
#include <QUuid>

#include <KCalendarCore/Event>
#include <KCalendarCore/ICalFormat>
#include <KCalendarCore/OccurrenceIterator>

namespace
{
constexpr int FlagAllDay = 8;
constexpr int FlagRecurrenceIdAllDay = 512;

QString icsEscape(QString s)
{
    s.replace(QLatin1Char('\\'), QLatin1String("\\\\"));
    s.replace(QLatin1Char(';'), QLatin1String("\\;"));
    s.replace(QLatin1Char(','), QLatin1String("\\,"));
    s.replace(QLatin1String("\r\n"), QLatin1String("\n"));
    s.replace(QLatin1Char('\n'), QLatin1String("\\n"));
    s.remove(QLatin1Char('\r'));
    return s;
}

// Thunderbird stores instants as microseconds since 1970 (UTC); "floating" and all-day
// events are stored as UTC wall-clock time.
QString dtLine(const QString &name, qint64 us, const QString &tz, bool allDay)
{
    const QDateTime utc = QDateTime::fromMSecsSinceEpoch(us / 1000, QTimeZone::utc());
    if (allDay) {
        return name + QStringLiteral(";VALUE=DATE:") + utc.date().toString(QStringLiteral("yyyyMMdd"));
    }
    const QString fmt = QStringLiteral("yyyyMMdd'T'HHmmss");
    if (tz == QLatin1String("UTC")) {
        return name + QLatin1Char(':') + utc.toString(fmt) + QLatin1Char('Z');
    }
    if (tz.isEmpty() || tz == QLatin1String("floating")) {
        return name + QLatin1Char(':') + utc.toString(fmt);
    }
    const QTimeZone zone(tz.toUtf8());
    if (!zone.isValid()) {
        return name + QLatin1Char(':') + utc.toString(fmt) + QLatin1Char('Z');
    }
    return name + QStringLiteral(";TZID=") + tz + QLatin1Char(':') + utc.toTimeZone(zone).toString(fmt);
}

// Google events created from e-mails (e.g. Booking) only carry a long notice with links as
// description, and other descriptions can be several KB long. An excerpt is enough for the clock.
QString cleanDescription(QString d)
{
    if (d.contains(QLatin1String("aus einer E-Mail erstellt")) || d.contains(QLatin1String("automatisch erstellten Terminen"))
        || d.contains(QLatin1String("created from an email")) || d.contains(QLatin1String("automatically created events"))) {
        return {};
    }
    d = d.simplified();
    constexpr int maxLen = 200;
    if (d.size() > maxLen) {
        d = d.left(maxLen).trimmed() + QStringLiteral("…");
    }
    return d;
}

QString prefValue(const QString &raw)
{
    QString v = raw.trimmed();
    if (v.startsWith(QLatin1Char('"')) && v.endsWith(QLatin1Char('"')) && v.size() >= 2) {
        v = v.mid(1, v.size() - 2);
        v.replace(QStringLiteral("\\\""), QStringLiteral("\""));
        v.replace(QStringLiteral("\\\\"), QStringLiteral("\\"));
    }
    return v;
}
}

TbEvents::TbEvents() = default;

QString TbEvents::findCachePath()
{
    // Thunderbird keeps its profiles in ~/.thunderbird (classic), ~/.config/thunderbird (XDG)
    // or inside the Flatpak sandbox, depending on version and packaging.
    const QString home = QDir::homePath();
    const QStringList roots = {home + QStringLiteral("/.thunderbird"),
                               home + QStringLiteral("/.config/thunderbird"),
                               home + QStringLiteral("/.var/app/org.mozilla.Thunderbird/.thunderbird")};
    QString best;
    QDateTime bestTime;
    QFileInfoList profiles;
    for (const QString &rootPath : roots) {
        profiles += QDir(rootPath).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    }
    for (const QFileInfo &profile : std::as_const(profiles)) {
        const QFileInfo cache(profile.absoluteFilePath() + QStringLiteral("/calendar-data/cache.sqlite"));
        if (!cache.exists()) {
            continue;
        }
        const QFileInfo wal(cache.absoluteFilePath() + QStringLiteral("-wal"));
        QDateTime t = cache.lastModified();
        if (wal.exists() && wal.lastModified() > t) {
            t = wal.lastModified();
        }
        if (best.isEmpty() || t > bestTime) {
            best = cache.absoluteFilePath();
            bestTime = t;
        }
    }
    return best;
}

QHash<QString, TbEvents::CalendarInfo> TbEvents::readCalendarInfos(const QString &profileDir) const
{
    QHash<QString, QHash<QString, QString>> raw;
    QFile prefs(profileDir + QStringLiteral("/prefs.js"));
    if (prefs.open(QIODevice::ReadOnly)) {
        static const QRegularExpression re(
            QStringLiteral(R"re(user_pref\("calendar\.registry\.([0-9a-fA-F-]+)\.([A-Za-z-]+)",\s*(.*)\);)re"));
        while (!prefs.atEnd()) {
            const QString line = QString::fromUtf8(prefs.readLine());
            const auto m = re.match(line);
            if (m.hasMatch()) {
                raw[m.captured(1)][m.captured(2)] = prefValue(m.captured(3));
            }
        }
    }

    QHash<QString, CalendarInfo> result;
    for (auto it = raw.cbegin(); it != raw.cend(); ++it) {
        const auto &p = it.value();
        if (p.value(QStringLiteral("disabled")) == QLatin1String("true")) {
            continue;
        }
        if (p.value(QStringLiteral("type")) == QLatin1String("storage") && p.value(QStringLiteral("name")).isEmpty()) {
            continue;
        }
        CalendarInfo info;
        info.name = p.value(QStringLiteral("name"));
        info.color = p.value(QStringLiteral("color"));
        // Holidays are already provided by the Plasma "Holidays" plugin.
        if (info.name.contains(QStringLiteral("Feiertage"), Qt::CaseInsensitive) || info.name.contains(QStringLiteral("holiday"), Qt::CaseInsensitive)) {
            continue;
        }
        result.insert(it.key(), info);
    }
    return result;
}

QString TbEvents::buildIcs(QSqlDatabase &db, const QHash<QString, CalendarInfo> &calendars)
{
    // Description and location live in cal_properties.
    QHash<QString, QString> props;
    {
        QSqlQuery q(db);
        q.exec(QStringLiteral("SELECT cal_id, item_id, key, value, recurrence_id FROM cal_properties "
                              "WHERE key IN ('DESCRIPTION','LOCATION')"));
        while (q.next()) {
            const QString key = q.value(0).toString() + QLatin1Char('|') + q.value(1).toString() + QLatin1Char('|')
                + (q.value(4).isNull() ? QString() : q.value(4).toString()) + QLatin1Char('|') + q.value(2).toString();
            props.insert(key, QString::fromUtf8(q.value(3).toByteArray()));
        }
    }

    // Recurrence rules, exception dates etc. are already stored as iCal lines in cal_recurrence.
    QHash<QString, QStringList> recurrence;
    {
        QSqlQuery q(db);
        q.exec(QStringLiteral("SELECT cal_id, item_id, icalString FROM cal_recurrence"));
        while (q.next()) {
            const QString key = q.value(0).toString() + QLatin1Char('|') + q.value(1).toString();
            const auto lines = q.value(2).toString().split(QLatin1Char('\n'), Qt::SkipEmptyParts);
            for (const QString &l : lines) {
                const QString t = l.trimmed();
                if (!t.isEmpty()) {
                    recurrence[key].append(t);
                }
            }
        }
    }

    QString ics;
    ics.reserve(4 * 1024 * 1024);
    ics += QStringLiteral("BEGIN:VCALENDAR\r\nVERSION:2.0\r\nPRODID:-//tbcalendarplugin//EN\r\n");

    QSqlQuery q(db);
    q.exec(QStringLiteral("SELECT cal_id, id, title, flags, event_start, event_end, event_start_tz, event_end_tz, "
                          "recurrence_id, recurrence_id_tz, ical_status FROM cal_events"));
    while (q.next()) {
        const QString calId = q.value(0).toString();
        if (!calendars.contains(calId)) {
            continue;
        }
        const QString status = q.value(10).toString();
        if (status == QLatin1String("CANCELLED")) {
            continue;
        }
        const QString id = q.value(1).toString();
        const int flags = q.value(3).toInt();
        const bool allDay = flags & FlagAllDay;
        const QString rid = q.value(8).isNull() ? QString() : q.value(8).toString();

        ics += QStringLiteral("BEGIN:VEVENT\r\n");
        ics += QStringLiteral("UID:") + calId + QLatin1Char('|') + icsEscape(id) + QStringLiteral("\r\n");
        ics += QStringLiteral("DTSTAMP:20000101T000000Z\r\n");
        ics += dtLine(QStringLiteral("DTSTART"), q.value(4).toLongLong(), q.value(6).toString(), allDay) + QStringLiteral("\r\n");
        ics += dtLine(QStringLiteral("DTEND"), q.value(5).toLongLong(), q.value(7).toString(), allDay) + QStringLiteral("\r\n");
        ics += QStringLiteral("SUMMARY:") + icsEscape(q.value(2).toString()) + QStringLiteral("\r\n");
        if (!rid.isEmpty()) {
            const bool ridAllDay = flags & FlagRecurrenceIdAllDay;
            ics += dtLine(QStringLiteral("RECURRENCE-ID"), rid.toLongLong(), q.value(9).toString(), ridAllDay) + QStringLiteral("\r\n");
        }
        const QString base = calId + QLatin1Char('|') + id + QLatin1Char('|') + rid + QLatin1Char('|');
        for (const char *key : {"DESCRIPTION", "LOCATION"}) {
            const QString v = props.value(base + QLatin1String(key));
            if (!v.isEmpty()) {
                ics += QLatin1String(key) + QLatin1Char(':') + icsEscape(v) + QStringLiteral("\r\n");
            }
        }
        if (rid.isEmpty()) {
            const auto lines = recurrence.value(calId + QLatin1Char('|') + id);
            for (const QString &l : lines) {
                ics += l + QStringLiteral("\r\n");
            }
        }
        ics += QStringLiteral("END:VEVENT\r\n");
    }
    ics += QStringLiteral("END:VCALENDAR\r\n");
    return ics;
}

bool TbEvents::reload(const QString &cachePath)
{
    if (!m_tmp.isValid()) {
        return false;
    }
    const QString dst = m_tmp.path() + QStringLiteral("/cache.sqlite");
    QFile::remove(dst);
    QFile::remove(dst + QStringLiteral("-wal"));
    QFile::remove(dst + QStringLiteral("-shm"));
    // Copy including the WAL, since Thunderbird often keeps new events only in the write-ahead log.
    if (!QFile::copy(cachePath, dst)) {
        return false;
    }
    if (QFile::exists(cachePath + QStringLiteral("-wal"))) {
        QFile::copy(cachePath + QStringLiteral("-wal"), dst + QStringLiteral("-wal"));
    }

    const QString profileDir = QFileInfo(cachePath).dir().absoluteFilePath(QStringLiteral(".."));
    m_calendars = readCalendarInfos(QDir::cleanPath(profileDir));

    QString ics;
    const QString conn = QStringLiteral("tbevents-") + QUuid::createUuid().toString(QUuid::WithoutBraces);
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), conn);
        db.setDatabaseName(dst);
        if (!db.open()) {
            db = QSqlDatabase();
            QSqlDatabase::removeDatabase(conn);
            return false;
        }
        ics = buildIcs(db, m_calendars);
        db.close();
    }
    QSqlDatabase::removeDatabase(conn);

    auto cal = KCalendarCore::MemoryCalendar::Ptr::create(QTimeZone::systemTimeZone());
    KCalendarCore::ICalFormat format;
    if (!format.fromString(cal, ics)) {
        return false;
    }
    m_calendar = cal;
    return true;
}

QMultiHash<QDate, CalendarEvents::EventData> TbEvents::eventsFor(const QDate &start, const QDate &end)
{
    QMutexLocker lock(&m_mutex);
    QMultiHash<QDate, CalendarEvents::EventData> result;

    const QString cachePath = findCachePath();
    if (cachePath.isEmpty()) {
        return result;
    }
    const QFileInfo db(cachePath);
    const QFileInfo wal(cachePath + QStringLiteral("-wal"));
    const QString signature = cachePath + QLatin1Char('|') + QString::number(db.size()) + QLatin1Char('|')
        + QString::number(db.lastModified().toMSecsSinceEpoch()) + QLatin1Char('|') + QString::number(wal.exists() ? wal.size() : -1)
        + QLatin1Char('|') + QString::number(wal.exists() ? wal.lastModified().toMSecsSinceEpoch() : 0);
    if (signature != m_signature || !m_calendar) {
        if (!reload(cachePath)) {
            return result;
        }
        m_signature = signature;
    }

    KCalendarCore::OccurrenceIterator it(*m_calendar, QDateTime(start, QTime(0, 0, 0)), QDateTime(end, QTime(23, 59, 59)));
    while (it.hasNext()) {
        it.next();
        const auto event = it.incidence().dynamicCast<KCalendarCore::Event>();
        if (!event) {
            continue;
        }
        const QDateTime occStart = it.occurrenceStartDate();
        const bool allDay = event->allDay();
        QDateTime occEnd = event->endDateForStart(occStart);
        if (!occEnd.isValid()) {
            occEnd = occStart;
        }

        QDate firstDay;
        QDate lastDay;
        QDateTime startTime;
        QDateTime endTime;
        if (allDay) {
            firstDay = occStart.date();
            lastDay = occEnd.date();
            startTime = QDateTime(firstDay, QTime(0, 0, 0));
            endTime = QDateTime(lastDay.addDays(1), QTime(0, 0, 0));
        } else {
            startTime = occStart.toLocalTime();
            endTime = occEnd.toLocalTime();
            firstDay = startTime.date();
            lastDay = endTime.date();
            if (endTime.time() == QTime(0, 0, 0) && endTime > startTime) {
                lastDay = lastDay.addDays(-1);
            }
        }
        if (lastDay < firstDay) {
            lastDay = firstDay;
        }

        const QString uid = event->uid();
        const QString calId = uid.section(QLatin1Char('|'), 0, 0);

        CalendarEvents::EventData data;
        data.setTitle(event->summary());
        data.setDescription(cleanDescription(event->description()));
        data.setLocation(event->location());
        data.setEventType(CalendarEvents::EventData::Event);
        data.setIsAllDay(allDay);
        data.setStartDateTime(startTime);
        data.setEndDateTime(endTime);
        data.setEventColor(m_calendars.value(calId).color);
        data.setUid(QStringLiteral("thunderbird:") + uid + QLatin1Char(':') + occStart.toString(Qt::ISODate));

        const int maxDays = 400;
        int n = 0;
        for (QDate d = qMax(firstDay, start); d <= lastDay && d <= end && n < maxDays; d = d.addDays(1), ++n) {
            result.insert(d, data);
        }
    }
    return result;
}
