// ============================================================================
//  bootsplash.h —— C++ 原生开机闪屏 (纯 QGui 栈, 零新增依赖)
//
//  为什么必须在 C++ 层: QML 的 bootMask 在部分机器上到不了首帧
//  (卡在 QML 引擎加载/OpenGL 上下文之前, 窗口全白)。本闪屏用
//  QRasterWindow + QPainter 手绘, 只需 QGuiApplication, 毫秒级出现,
//  主窗口首帧渲染完成后关闭。
//
//  注意: 不用 QSplashScreen —— 它属于 QtWidgets 模块, 绿包没有
//  Qt6Widgets.dll, 引入会增加部署体积。
// ============================================================================

#pragma once

#include <QRasterWindow>
#include <QPixmap>
#include <QTimer>

class BootSplash : public QRasterWindow
{
    Q_OBJECT
public:
    // bg 为空时用纯色深空底 + 文字, 不崩。
    // 自检/基准模式 (headless) 下构造后保持隐藏, 调用方照常调 finish 即可。
    explicit BootSplash(const QPixmap &bg, bool headless,
                        QRasterWindow *parent = nullptr);

    // 阶段文案推进 (QML 加载各阶段调用, 每调一次重绘)。
    void setStage(const QString &text);

    // 加载进度 0..1 (各阶段调用; 显示值平滑追踪目标值, 条带动画持续重绘)。
    void setProgress(double v);

    // 淡出关闭 (场景真正就绪后调用; 重复调用安全)。
    void finish();

protected:
    void paintEvent(QPaintEvent *) override;

private:
    QPixmap m_bg;
    QString m_stage = QStringLiteral("正在启动…");
    double  m_opacity = 1.0;
    // 进度条: 目标值 (外部设) + 显示值 (每 100ms 追踪) +  marching 高光计时。
    double  m_prog = 0.0;
    double  m_target = 0.0;
    int     m_tick = 0;
    bool    m_finishing = false;
    QTimer *m_fadeTimer = nullptr;
    QTimer *m_animTimer = nullptr;
};
