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

// Liest die Termine, die Thunderbird in calendar-data/cache.sqlite ablegt, und stellt sie
// für beliebige Datumsbereiche bereit. Alle Zugriffe sind lesend; die Datenbank wird vorher
// kopiert, damit Thunderbird nicht gestört wird.
class TbEvents
{
public:
    TbEvents();

    // Blockierend, thread-sicher. Lädt die Datenbank neu, wenn Thunderbird sie verändert hat.
    QMultiHash<QDate, CalendarEvents::EventData> eventsFor(const QDate &start, const QDate &end);

    // Pfad der cache.sqlite des zuletzt benutzten Thunderbird-Profils, leer wenn keiner da ist.
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
