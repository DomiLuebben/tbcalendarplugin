// SPDX-FileCopyrightText: 2026 tbcalendarplugin contributors
// SPDX-License-Identifier: GPL-3.0-only

#include <QDebug>
#include <QtConcurrent>

#include <CalendarEvents/CalendarEventsPlugin>

#include "tbevents.h"

// Shows events from Thunderbird's calendar cache in the Plasma clock. Loading runs in its own
// thread so that plasmashell never has to wait for this data.
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
        QPointer<ThunderbirdEventsPlugin> self(this);
        (void)QtConcurrent::run([self, startDate, endDate] {
            if (!self) {
                return;
            }
            const auto events = self->m_events.eventsFor(startDate, endDate);
            qInfo().noquote() << "thunderbirdevents:" << events.size() << "events for" << startDate.toString(Qt::ISODate) << "to" << endDate.toString(Qt::ISODate);
            QMetaObject::invokeMethod(
                qApp,
                [self, events] {
                    if (self) {
                        Q_EMIT self->dataReady(events);
                    }
                },
                Qt::QueuedConnection);
        });
    }

private:
    TbEvents m_events;
};

#include "plugin.moc"
