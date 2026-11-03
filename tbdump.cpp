#include <QCoreApplication>
#include <QElapsedTimer>
#include <QTextStream>

#include <algorithm>

#include "tbevents.h"

// Testprogramm: tbdump [von bis]   (Format YYYY-MM-DD, Standard: heute bis heute+14)
int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTextStream out(stdout);
    QDate from = QDate::currentDate();
    QDate to = from.addDays(14);
    if (argc >= 3) {
        from = QDate::fromString(QString::fromLocal8Bit(argv[1]), Qt::ISODate);
        to = QDate::fromString(QString::fromLocal8Bit(argv[2]), Qt::ISODate);
    }

    out << "Cache: " << TbEvents::findCachePath() << "\n";
    TbEvents events;
    QElapsedTimer timer;
    timer.start();
    const auto result = events.eventsFor(from, to);
    out << "Ladezeit: " << timer.elapsed() << " ms, " << result.size() << " Einträge\n";

    QList<QDate> days = result.uniqueKeys();
    std::sort(days.begin(), days.end());
    for (const QDate &d : std::as_const(days)) {
        auto list = result.values(d);
        std::sort(list.begin(), list.end(), [](const auto &a, const auto &b) { return a.startDateTime() < b.startDateTime(); });
        for (const auto &e : std::as_const(list)) {
            out << d.toString(Qt::ISODate) << "  "
                << (e.isAllDay() ? QStringLiteral("ganztägig  ") : e.startDateTime().time().toString(QStringLiteral("HH:mm")) + QStringLiteral("-")
                        + e.endDateTime().time().toString(QStringLiteral("HH:mm")) + QStringLiteral("  "))
                << e.title() << "  [" << e.eventColor() << "]";
            if (!e.location().isEmpty()) {
                out << "  @ " << e.location();
            }
            if (!e.description().isEmpty()) {
                out << "  | " << e.description().left(60);
            }
            out << "\n";
        }
    }
    return 0;
}
