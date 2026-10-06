#ifndef DATACURVE_H
#define DATACURVE_H

#include <QWidget>
#include <QVector>
#include <QPoint>
#include <QTime>

// 自绘实时滚动曲线控件（零第三方依赖）
// 对应简历关键词：QPainter、数据可视化、实时刷新
class DataCurve : public QWidget
{
    Q_OBJECT
public:
    explicit DataCurve(QWidget *parent = nullptr);

    void setMaxPoints(int n);
    void appendValue(double v);
    void setYRange(double min, double max);
    void setTitle(const QString &t);
    void setLineColor(const QColor &c);
    void clear() { m_data.clear(); update(); }
    void setStartTime(const QTime &t) { m_startTime = t; }

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

private:
    QVector<double> m_data;
    int m_maxPoints = 200;
    double m_yMin = 0.0;
    double m_yMax = 100.0;
    QString m_title;
    QColor m_color = QColor(0, 120, 215);
    QTime m_startTime;
    QPoint m_mousePos;
};

#endif // DATACURVE_H
