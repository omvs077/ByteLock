#include "ui/IconUtils.h"

#include <QPainter>
#include <QPixmap>
#include <QSvgRenderer>

QIcon loadSvgIcon(const QString& resourcePath, const QSize& size)
{
    QSvgRenderer renderer(resourcePath);

    QPixmap pixmap(size);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    renderer.render(&painter);
    painter.end();

    return QIcon(pixmap);
}
