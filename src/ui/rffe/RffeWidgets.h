// RFFE widget kit.
//
// The small set of styled widgets the original app's RFFE panel/dialogs are built from
// (RffeCard, RffeBrand, RffeButton, RffeCombo, RffeCountChip, RffeSegmented, RffeStatusLed,
// RffeTerminal). Colours come from the rffe::Palette tokens; the class names double as QSS
// selectors so a single panel stylesheet covers them.
#pragma once

#include "core/rffe/RffeTheme.h"

#include <QComboBox>
#include <QFrame>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>
#include <QVBoxLayout>
#include <QVector>
#include <QWidget>

class QPainter;

namespace w2r {
namespace rffe {

// A rounded card container; children go into body(). An optional title row is added first.
class RffeCard : public QFrame {
	Q_OBJECT
  public:
	explicit RffeCard(const Palette &pal, const QString &title = QString(), QWidget *parent = nullptr);
	QVBoxLayout *body() const { return m_body; }

  private:
	QVBoxLayout *m_body = nullptr;
};

// "W2R SCHEMATICS" brand lockup.
class RffeBrand : public QWidget {
	Q_OBJECT
  public:
	explicit RffeBrand(QWidget *parent = nullptr);
};

// A styled push button. Primary/danger map to the accent/danger tokens.
class RffeButton : public QPushButton {
	Q_OBJECT
  public:
	enum Variant { Normal, Primary, Danger };
	explicit RffeButton(const QString &text, Variant variant = Normal, QWidget *parent = nullptr);
	void setVariant(Variant variant);
};

// A styled combo box; its popup list is objectName rffeComboPopup.
class RffeCombo : public QComboBox {
	Q_OBJECT
  public:
	explicit RffeCombo(const Palette &pal, QWidget *parent = nullptr);
};

// Paints combo popup rows with the palette.
class RffeComboDelegate : public QStyledItemDelegate {
	Q_OBJECT
  public:
	explicit RffeComboDelegate(const Palette &pal, QObject *parent = nullptr);
	void paint(QPainter *painter, const QStyleOptionViewItem &option,
	           const QModelIndex &index) const override;

  private:
	Palette m_pal;
};

// A small pill badge (e.g. "12 of 40 pads mapped on this board").
class RffeCountChip : public QLabel {
	Q_OBJECT
  public:
	explicit RffeCountChip(QWidget *parent = nullptr);
	void setCount(int current, int total, const QString &noun);
};

// A segmented control (e.g. Model | Bus). Emits currentChanged(index).
class RffeSegmented : public QWidget {
	Q_OBJECT
  public:
	explicit RffeSegmented(QWidget *parent = nullptr);
	int addSegment(const QString &label);
	int currentIndex() const { return m_current; }
	void setCurrentIndex(int index);

  signals:
	void currentChanged(int index);

  private:
	QVector<QPushButton *> m_buttons;
	int m_current = -1;
};

// A status LED dot with a soft glow.
class RffeStatusLed : public QWidget {
	Q_OBJECT
  public:
	enum State { Off, On, Danger, Scan };
	explicit RffeStatusLed(const Palette &pal, QWidget *parent = nullptr);
	void setState(State state);
	State state() const { return m_state; }
	QSize sizeHint() const override { return QSize(16, 16); }

  protected:
	void paintEvent(QPaintEvent *event) override;

  private:
	Palette m_pal;
	State m_state = Off;
};

// A monospace log with coloured lines.
class RffeTerminal : public QPlainTextEdit {
	Q_OBJECT
  public:
	explicit RffeTerminal(const Palette &pal, QWidget *parent = nullptr);
	void appendLine(const QString &text);
	void appendOk(const QString &text);
	void appendWarn(const QString &text);
	void appendError(const QString &text);

  private:
	void appendColored(const QString &text, const QColor &color);
	Palette m_pal;
};

} // namespace rffe
} // namespace w2r
