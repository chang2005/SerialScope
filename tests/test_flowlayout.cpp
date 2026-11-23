#include "ui/FlowLayout.h"

#include <QLabel>
#include <QPushButton>
#include <QTest>
#include <QWidget>

class TestFlowLayout : public QObject
{
    Q_OBJECT
private slots:
    void laysOutWidgets();
    void wrapsWhenNarrow();
    void reportsHeightForWidth();
};

void TestFlowLayout::laysOutWidgets()
{
    QWidget host;
    auto *layout = new FlowLayout(&host);
    layout->setContentsMargins(0, 0, 0, 0);

    auto *a = new QPushButton(QStringLiteral("AAAA"), &host);
    auto *b = new QPushButton(QStringLiteral("BBBB"), &host);
    layout->addWidget(a);
    layout->addWidget(b);

    host.resize(800, 100);
    host.show();
    QVERIFY(QTest::qWaitForWindowExposed(&host));

    QCOMPARE(layout->count(), 2);
    QVERIFY(layout->heightForWidth(800) > 0);

    // 两个按钮应在同一行（宽度足够时）
    const int aBottom = a->geometry().bottom();
    const int bBottom = b->geometry().bottom();
    QCOMPARE(aBottom, bBottom);
}

void TestFlowLayout::wrapsWhenNarrow()
{
    QWidget host;
    auto *layout = new FlowLayout(&host);
    layout->setContentsMargins(0, 0, 0, 0);

    auto *a = new QPushButton(QStringLiteral("AAAAAAAAAA"), &host);
    auto *b = new QPushButton(QStringLiteral("BBBBBBBBBB"), &host);
    layout->addWidget(a);
    layout->addWidget(b);

    host.resize(200, 200);
    host.show();
    QVERIFY(QTest::qWaitForWindowExposed(&host));

    // 宽度不够时第二个按钮应换行
    QVERIFY(b->geometry().top() > a->geometry().top());
    QVERIFY(layout->heightForWidth(200) > layout->heightForWidth(800));
}

void TestFlowLayout::reportsHeightForWidth()
{
    QWidget host;
    auto *layout = new FlowLayout(&host);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(new QPushButton(QStringLiteral("X"), &host));

    QVERIFY(layout->hasHeightForWidth());
    QVERIFY(layout->heightForWidth(400) > 0);
    QVERIFY(host.sizeHint().height() > 0);
}

QTEST_MAIN(TestFlowLayout)
#include "test_flowlayout.moc"
