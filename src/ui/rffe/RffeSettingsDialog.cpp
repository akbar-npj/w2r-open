#include "ui/rffe/RffeSettingsDialog.h"

#include "ui/rffe/RffeWidgets.h"

#include <QCheckBox>
#include <QFont>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace w2r {
namespace rffe {

namespace {
int clampIfPositive(int value, int lo, int hi)
{
	if (value <= 0) return value;
	return value < lo ? lo : (value > hi ? hi : value);
}
} // namespace

RffeSettingsDialog::RffeSettingsDialog(const Palette &pal, const MarkSettings &initial,
                                       QWidget *parent)
	: QDialog(parent), m_settings(initial)
{
	setWindowTitle(tr("RFFE Settings"));
	resize(440, 360);
	// Panel sheet (buttons, combos) + the recovered dialog sheet (card, labels, checkboxes).
	setStyleSheet(pal.styleSheet() + pal.settingsDialogStyleSheet());

	auto *root = new QVBoxLayout(this);
	root->setContentsMargins(12, 12, 12, 12);

	auto *card = new RffeCard(pal, QString(), this);
	card->setObjectName(QStringLiteral("rffeSettingsCard"));
	auto *body = card->body();

	auto *title = new QLabel(tr("RFFE settings"), card);
	title->setObjectName(QStringLiteral("rffeSettingsTitle"));
	QFont tf = title->font();
	tf.setBold(true);
	title->setFont(tf);
	body->addWidget(title);

	auto *note = new QLabel(tr("Marking and animation options for RFFE nets."), card);
	note->setObjectName(QStringLiteral("rffeNote"));
	note->setWordWrap(true);
	note->setStyleSheet(QStringLiteral("color:%1;").arg(pal.textFaint.name()));
	body->addWidget(note);

	auto *form = new QFormLayout();

	m_blinkMs = new QSpinBox(card);
	m_blinkMs->setRange(0, 1400);
	m_blinkMs->setSingleStep(50);
	m_blinkMs->setSuffix(QStringLiteral(" ms"));
	m_blinkMs->setValue(initial.markBlinkMs);
	form->addRow(tr("Net & component highlight blink"), m_blinkMs);

	m_holdMs = new QSpinBox(card);
	m_holdMs->setRange(0, 120000);
	m_holdMs->setSingleStep(500);
	m_holdMs->setSuffix(QStringLiteral(" ms"));
	m_holdMs->setValue(initial.markHoldMs);
	form->addRow(tr("Hold mark for"), m_holdMs);

	m_halo = new QCheckBox(tr("Halo ring around marked pads"), card);
	m_halo->setChecked(initial.markHalo);
	form->addRow(QString(), m_halo);

	m_esp32 = new QCheckBox(tr("Probe is an ESP32"), card);
	m_esp32->setChecked(initial.esp32);
	form->addRow(QString(), m_esp32);

	m_beep = new QCheckBox(tr("Beep when a scan or update starts and ends"), card);
	m_beep->setChecked(initial.beep);
	form->addRow(QString(), m_beep);

	body->addLayout(form);

	auto *buttons = new QHBoxLayout();
	buttons->addStretch(1);
	auto *cancel = new RffeButton(tr("Cancel"), RffeButton::Normal, this);
	connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
	buttons->addWidget(cancel);
	auto *ok = new RffeButton(tr("OK"), RffeButton::Primary, this);
	connect(ok, &QPushButton::clicked, this, [this] { accept(); });
	buttons->addWidget(ok);
	root->addWidget(card, 1);
	root->addLayout(buttons);
}

void RffeSettingsDialog::accept()
{
	m_settings.markBlinkMs = clampIfPositive(m_blinkMs->value(), 150, 1400);
	m_settings.markHoldMs = clampIfPositive(m_holdMs->value(), 1000, 120000);
	m_settings.markHalo = m_halo->isChecked();
	m_settings.esp32 = m_esp32->isChecked();
	m_settings.beep = m_beep->isChecked();
	QDialog::accept();
}

} // namespace rffe
} // namespace w2r
