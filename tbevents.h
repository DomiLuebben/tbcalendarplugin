// SPDX-FileCopyrightText: 2026 tbcalendarplugin contributors
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QDate>
#include <QDateTime>
#include <QHash>
#include <QMultiHash>
#include <QMutex>
#include <QString>
#include <QTemporaryDir>

#include <CalendarEvents/CalendarEventsPlugin>
#include <KCalendarCore/MemoryCalendar>

// Reads the events Thunderbird keeps in calendar-data/cache.sqlite and serves them for
// arbitrary date ranges. Access is read-only: the database is copied first so Thunderbird
// is never disturbed.
class TbEvents
{
public:
    TbEvents();

    // Blocking and thread-safe. Reloads the database when Thunderbird has changed it.
    QMultiHash<QDate, CalendarEvents::EventData> eventsFor(const QDate &start, const QDate &end);

    // Path of the cache.sqlite of the most recently used Thunderbird profile, empty if none.
    static QString findCachePath();

private:
    struct CalendarInfo {
        QString name;
        QString color;
    };

    bool reload(const QString &cachePath);
    QHash<QString, CalendarInfo> readCalendarInfos(const QString &profileDir) const;
    static QString buildIcs(class QSqlDatabase &db, const QHash<QString, CalendarInfo> &calendars);

    QMutex m_mutex;
    QTemporaryDir m_tmp;
    QString m_signature;
    QHash<QString, CalendarInfo> m_calendars;
    KCalendarCore::MemoryCalendar::Ptr m_calendar;
};
