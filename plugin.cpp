// SPDX-FileCopyrightText: 2026 tbcalendarplugin contributors
// SPDX-License-Identifier: GPL-3.0-only

#include <QDebug>

#include <CalendarEvents/CalendarEventsPlugin>

#include "tbevents.h"

// Shows events from Thunderbird's calendar cache in the Plasma clock.
//
// The result is emitted synchronously from inside loadEventsForDateRange(). Plasma's DaysModel
// clears its events right before it asks the plugins and then appends whatever dataReady()
// delivers, without removing duplicates. Every model receives every emission, so an answer that
// arrives later than another model's request ends up in the calendar twice. Reloading the cache
// only takes about 100 ms and only happens when Thunderbird has changed it, so staying
// synchronous is the safe choice.
class ThunderbirdEventsPlugin : public CalendarEvents::CalendarEventsPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.kde.CalendarEventsPlugin" FILE "thunderbirdevents.json")
    Q_INTERFACES(CalendarEvents::CalendarEventsPlugin)

public:
    explicit ThunderbirdEventsPlugin(QObject *parent = nullptr)
        : CalendarEvents::CalendarEventsPlugin(parent)
    {
    }

    void loadEventsForDateRange(const QDate &startDate, const QDate &endDate) override
    {
        const auto events = m_events.eventsFor(startDate, endDate);
        qInfo().noquote() << "thunderbirdevents:" << events.size() << "events for" << startDate.toString(Qt::ISODate) << "to" << endDate.toString(Qt::ISODate);
        Q_EMIT dataReady(events);
    }

private:
    TbEvents m_events;
};

#include "plugin.moc"
