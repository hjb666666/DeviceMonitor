#include "datacurve.h"
#include <QPainter>
#include <QPen>
#include <QFont>
#include <QMouseEvent>
#include <QToolTip>

DataCurve::DataCurve(QWidget *parent) : QWidget(parent)
{
    setMinimumHeight(180);
    setAutoFillBackground(true);
    setPalette(QPalette(QColor(250, 250, 250)));
    setMouseTracking(true);
}

void DataCurve::setMaxPoints(int n) { m_maxPoints = n; }
void DataCurve::setYRange(double min, double max) { m_yMin = min; m_yMax = max; update(); }
void DataCurve::setTitle(const QString &t) { m_title = t; update(); }
void DataCurve::setLineColor(const QColor &c) { m_color = c; update(); }

void DataCurve::appendValue(double v)
{
    m_data.append(v);
    if (m_data.size() > m_maxPoints)
        m_data.removeFirst();
    update();
}

void DataCurve::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    p.fillRect(rect(), QColor(250, 250, 250));

    // 标题
    p.setPen(Qt::black);
    p.setFont(QFont("Microsoft YaHei", 9, QFont::Bold));
    p.drawText(10, 16, m_title);

    // 右上角显示当前值
    double current = m_data.isEmpty() ? 0 : m_data.last();
    p.setFont(QFont("Arial", 9, QFont::Bold));
    p.setPen(m_color);
    p.drawText(width() - 100, 16, QString::number(current, 'f', 1));

    QRect plotArea(45, 25, width() - 55, height() - 55);

    // 网格 + Y轴刻度
    p.setFont(QFont("Arial", 7));
    for (int i = 0; i <= 4; i++) {
        double val = m_yMax - (m_yMax - m_yMin) * i / 4.0;
        int y = plotArea.top() + plotArea.height() * i / 4;

        p.setPen(QPen(QColor(220, 220, 220), 1, Qt::DashLine));
        p.drawLine(plotArea.left(), y, plotArea.right(), y);

        p.setPen(Qt::gray);
        p.drawText(5, y + 4, QString::number(val, 'f', 0));
    }

    // X轴时间刻度（底部）
    p.setPen(Qt::gray);
    p.setFont(QFont("Arial", 7));
    int totalSeconds = m_startTime.isValid() ? m_startTime.secsTo(QTime::currentTime()) : 0;
    for (int i = 0; i <= 4; i++) {
        int x = plotArea.left() + plotArea.width() * i / 4;
        int sec = totalSeconds * i / 4;
        p.drawText(x - 15, plotArea.bottom() + 15,
                   QString("%1s").arg(sec));
    }

    // 边框
    p.setPen(QPen(Qt::gray, 1));
    p.drawRect(plotArea);

    if (m_data.size() < 2) return;

    // 曲线下填充
    QPainterPath fillPath;
    fillPath.moveTo(plotArea.left(), plotArea.bottom());
    for (int i = 0; i < m_data.size(); i++) {
        double v = m_data.at(i);
        double ratio = (v - m_yMin) / (m_yMax - m_yMin);
        int x = plotArea.left() + plotArea.width() * i / qMax(1, m_maxPoints - 1);
        int y = plotArea.bottom() - int(ratio * plotArea.height());
        fillPath.lineTo(x, y);
    }
    fillPath.lineTo(plotArea.left() + plotArea.width() * (m_data.size()-1) / qMax(1, m_maxPoints - 1), plotArea.bottom());
    fillPath.closeSubpath();
    p.fillPath(fillPath, QColor(m_color.red(), m_color.green(), m_color.blue(), 40));

    // 曲线
    QPainterPath path;
    for (int i = 0; i < m_data.size(); i++) {
        double v = m_data.at(i);
        double ratio = (v - m_yMin) / (m_yMax - m_yMin);
        int x = plotArea.left() + plotArea.width() * i / qMax(1, m_maxPoints - 1);
        int y = plotArea.bottom() - int(ratio * plotArea.height());
        if (i == 0) path.moveTo(x, y);
        else path.lineTo(x, y);
    }
    p.setPen(QPen(m_color, 2));
    p.drawPath(path);

    // 鼠标悬停竖线 + 数值
    if (m_mousePos.x() >= plotArea.left() && m_mousePos.x() <= plotArea.right()
        && m_mousePos.y() >= plotArea.top() && m_mousePos.y() <= plotArea.bottom()) {
        int idx = (m_mousePos.x() - plotArea.left()) * m_maxPoints / qMax(1, plotArea.width());
        idx = qBound(0, idx, m_data.size() - 1);

        double v = m_data.at(idx);
        int x = plotArea.left() + plotArea.width() * idx / qMax(1, m_maxPoints - 1);

        p.setPen(QPen(Qt::darkGray, 1, Qt::DashLine));
        p.drawLine(x, plotArea.top(), x, plotArea.bottom());

        p.setBrush(Qt::white);
        p.setPen(QPen(m_color, 2));
        p.drawEllipse(QPoint(x, plotArea.bottom() - int((v - m_yMin)/(m_yMax - m_yMin) * plotArea.height())), 4, 4);

        int sec = totalSeconds * idx / qMax(1, m_data.size() - 1);
        QToolTip::showText(mapToGlobal(m_mousePos),
            QString("时间: %1s\n%2: %3").arg(sec).arg(m_title).arg(v, 0, 'f', 1), this);
    }
}

void DataCurve::mouseMoveEvent(QMouseEvent *event)
{
    m_mousePos = event->pos();
    update();
}
