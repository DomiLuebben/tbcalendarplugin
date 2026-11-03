#include <QDebug>
#include <QtConcurrent>

#include <CalendarEvents/CalendarEventsPlugin>

#include "tbevents.h"

// Zeigt Termine aus dem Thunderbird-Kalender-Cache in der Plasma-Uhr. Das Laden läuft in einem
// eigenen Thread, damit Plasmashell nie auf diese Daten warten muss.
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
            qInfo().noquote() << "thunderbirdevents:" << events.size() << "Einträge für" << startDate.toString(Qt::ISODate) << "bis" << endDate.toString(Qt::ISODate);
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
