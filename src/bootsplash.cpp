// ============================================================================
//  bootsplash.cpp —— QRasterWindow 手绘闪屏实现
//
//  全屏覆盖主屏工作区 (杜绝主窗口首帧前白底从四周露出来) +
//  动态进度条 (平滑追踪 + marching 高光 + 百分比, 100ms 节拍持续重绘)。
// ============================================================================

#include "bootsplash.h"

#include <QPainter>
#include <QPaintEvent>
#include <QFont>
#include <QLinearGradient>
#include <QScreen>
#include <QGuiApplication>

BootSplash::BootSplash(const QPixmap &bg, bool headless, QRasterWindow *parent)
    : QRasterWindow(parent)
    , m_bg(bg)
{
    // ★ 置顶: 主窗口在 engine.load() 期间即被 OS 显示, 新窗口会抢到
    //   最前面盖住闪屏。置顶保证"白底主窗口"永远露不出来。
    setFlags(Qt::SplashScreen | Qt::FramelessWindowHint
             | Qt::WindowStaysOnTopHint);
    // ★ 全屏覆盖主屏工作区: 主窗口首帧前的白底不再从闪屏四周露出来。
    //   之前 900x560 居中, 四周一圈白, 用户误以为"闪屏和主界面之间有白屏"。
    if (QScreen *scr = QGuiApplication::primaryScreen()) {
        const QRect g = scr->availableGeometry();
        setGeometry(g);
    } else {
        resize(900, 560);
    }
    // 全局字体由 main.cpp 统一设置, 这里不再重复。
    // ★ 100ms 节拍: 进度平滑追踪 + marching 高光, 全程动态, 不靠外部调用。
    m_animTimer = new QTimer(this);
    connect(m_animTimer, &QTimer::timeout, this, [this] {
        if (m_finishing)
            return;
        if (m_prog < m_target) {
            const double gap = m_target - m_prog;
            m_prog = qMin(m_target, m_prog + qMax(0.02, gap * 0.2));
        } else if (m_prog < 0.95) {
            // 长耗时阶段 (GL 初始化/粒子构建) 缓慢爬行, 条带永不静止。
            m_prog = qMin(0.95, m_prog + 0.005);
        }
        ++m_tick;
        if (isVisible())
            update();
    });
    m_animTimer->start(100);
    if (!headless)
        show();
}

void BootSplash::setStage(const QString &text)
{
    m_stage = text;
    if (isVisible())
        update();
}

void BootSplash::setProgress(double v)
{
    m_target = qBound(0.0, v, 1.0);
    if (isVisible())
        update();
}

void BootSplash::finish()
{
    if (m_finishing)
        return;
    m_finishing = true;
    if (m_animTimer)
        m_animTimer->stop();
    m_prog = 1.0;
    if (isVisible())
        update();
    if (!isVisible()) {
        close();
        return;
    }
    // 8 帧淡出 (~130ms), 避免闪白
    m_fadeTimer = new QTimer(this);
    connect(m_fadeTimer, &QTimer::timeout, this, [this] {
        m_opacity -= 0.125;
        if (m_opacity <= 0.0) {
            m_fadeTimer->stop();
            close();
            return;
        }
        setOpacity(m_opacity);
        update();
    });
    m_fadeTimer->start(16);
}

void BootSplash::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const int W = width(), H = height();

    // ---- 背景: 有图铺图 (居中裁剪), 无图纯色 ----
    if (!m_bg.isNull()) {
        const QPixmap scaled = m_bg.scaled(
            QSize(W, H), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        const int x = (scaled.width() - W) / 2;
        const int y = (scaled.height() - H) / 2;
        p.drawPixmap(0, 0, W, H, scaled, x, y, W, H);
    } else {
        p.fillRect(0, 0, W, H, QColor(0x05, 0x07, 0x0d));
    }

    // ---- 左侧渐隐罩 (标题可读) ----
    QLinearGradient lg(0, 0, W, 0);
    lg.setColorAt(0.00, QColor(1, 2, 5, 210));
    lg.setColorAt(0.45, QColor(1, 2, 5, 115));
    lg.setColorAt(0.75, QColor(1, 2, 5, 12));
    lg.setColorAt(1.00, QColor(1, 2, 5, 0));
    p.fillRect(0, 0, W, H, lg);

    // ---- 底部渐隐罩 (加载区可读) ----
    QLinearGradient bg2(0, H * 0.62, 0, H);
    bg2.setColorAt(0.0, QColor(1, 2, 5, 0));
    bg2.setColorAt(1.0, QColor(1, 2, 5, 200));
    p.fillRect(0, 0, W, H, bg2);

    // ---- 标题 (与 QML bootMask 同版式, 首尾相接不跳跃) ----
    QFont t(QGuiApplication::font());
    t.setPixelSize(44);
    t.setBold(true);
    t.setLetterSpacing(QFont::AbsoluteSpacing, 6);
    p.setFont(t);
    p.setPen(QColor(0xf2, 0xf6, 0xff));
    p.drawText(72, 120, 460, 60, Qt::AlignLeft | Qt::AlignVCenter,
               QStringLiteral("宇宙实验室"));

    QFont s(t);
    s.setPixelSize(13);
    s.setLetterSpacing(QFont::AbsoluteSpacing, 2);
    p.setFont(s);
    p.setPen(QColor(158, 189, 235, 230));
    p.drawText(72, 172, 460, 24, Qt::AlignLeft | Qt::AlignVCenter,
               QStringLiteral("COSMOSLAB · 从太阳系到可观测宇宙"));
    p.fillRect(78, 206, 64, 2, QColor(0x5e, 0xa9, 0xff));

    // ---- 阶段文案 ----
    QFont st(t);
    st.setPixelSize(13);
    p.setFont(st);
    p.setPen(QColor(0xdb, 0xe6, 0xf7));
    p.drawText(72, H - 108, 380, 24, Qt::AlignLeft | Qt::AlignVCenter, m_stage);

    // ---- 动态进度条: 轨道 + 填充 + marching 高光 + 百分比 ----
    const int barX = 72, barY = H - 78, barW = 380, barH = 5;
    p.fillRect(barX, barY, barW, barH, QColor(255, 255, 255, 30));
    const int fillW = int(barW * qBound(0.0, m_prog, 1.0));
    if (fillW > 0)
        p.fillRect(barX, barY, fillW, barH, QColor(0x5e, 0xa9, 0xff));
    // marching 高光: 60px 亮段持续流动, 长耗时阶段也能看出"活着"。
    {
        const int segW = 60;
        const int span = barW + segW;
        const int hx = barX + (m_tick * 9) % span - segW;
        const int visX0 = qMax(hx, barX);
        const int visX1 = qMin(hx + segW, barX + barW);
        if (visX1 > visX0)
            p.fillRect(visX0, barY, visX1 - visX0, barH,
                       QColor(0xbd, 0xd9, 0xff, 160));
    }
    QFont pc(t);
    pc.setPixelSize(11);
    p.setFont(pc);
    p.setPen(QColor(140, 161, 191, 200));
    p.drawText(barX + barW + 12, barY - 4, 60, 14,
               Qt::AlignLeft | Qt::AlignVCenter,
               QStringLiteral("%1%").arg(int(qBound(0.0, m_prog, 1.0) * 100)));

    QFont v(t);
    v.setPixelSize(10);
    v.setFamily(QStringLiteral("Consolas"));
    p.setFont(v);
    p.setPen(QColor(140, 161, 191, 180));
    p.drawText(72, H - 50, 380, 20, Qt::AlignLeft | Qt::AlignVCenter,
               // ★ 版本号与发布包一致 (P1-2 修复: 曾硬编码 v2.0 与 Release v1.2 分裂)。
               QStringLiteral("v1.3 · C++ / QML / OpenGL"));
}
