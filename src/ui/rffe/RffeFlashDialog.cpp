#include "ui/rffe/RffeFlashDialog.h"

#include "core/rffe/RffeFlasher.h"
#include "ui/rffe/RffeWidgets.h"

#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollBar>
#include <QVBoxLayout>

namespace w2r {
namespace rffe {

namespace {
// Map an esptool output line to a friendly stage label (best effort; the tool is opaque).
QString stageForLine(const QString &line)
{
	const QString l = line.toLower();
	if (l.contains(QStringLiteral("connecting")) || l.contains(QStringLiteral("detecting chip")))
		return QObject::tr("Attach flash");
	if (l.contains(QStringLiteral("chip is"))) return QObject::tr("Attach flash");
	if (l.contains(QStringLiteral("changing baud")) || l.contains(QStringLiteral("baud")))
		return QObject::tr("Negotiating speed");
	if (l.contains(QStringLiteral("erasing"))) return QObject::tr("Erasing flash");
	if (l.contains(QStringLiteral("writing at")) || l.contains(QStringLiteral("wrote")))
		return QObject::tr("Writing firmware");
	if (l.contains(QStringLiteral("hash of data verified")) || l.contains(QStringLiteral("verif")))
		return QObject::tr("Verifying");
	if (l.contains(QStringLiteral("hard resetting")) || l.contains(QStringLiteral("resetting")))
		return QObject::tr("Restarting the probe");
	return QString();
}
} // namespace

RffeFlashDialog::RffeFlashDialog(const Palette &pal, RffeFlasher *flasher, const QString &port,
                                 QWidget *parent)
    : QDialog(parent), m_pal(pal), m_flasher(flasher), m_port(port)
{
	setWindowTitle(tr("Update Probe Firmware"));
	resize(520, 460);
	// Panel sheet (RffeButton) + the recovered dialog sheet (card, labels, progress, log).
	setStyleSheet(pal.styleSheet() + pal.flashDialogStyleSheet());

	auto *root = new QVBoxLayout(this);
	root->setContentsMargins(12, 12, 12, 12);

	auto *card = new RffeCard(pal, QString(), this);
	card->setObjectName(QStringLiteral("rffeFlashCard"));
	auto *body = card->body();

	auto *title = new QLabel(tr("Update Firmware"), card);
	title->setObjectName(QStringLiteral("rffeFlashTitle"));
	QFont tf = title->font();
	tf.setBold(true);
	title->setFont(tf);
	body->addWidget(title);

	m_stage = new QLabel(tr("Pick the probe's COM port first."), card);
	m_stage->setObjectName(QStringLiteral("rffeStage"));
	body->addWidget(m_stage);

	m_progress = new QProgressBar(card);
	m_progress->setRange(0, 100);
	m_progress->setValue(0);
	body->addWidget(m_progress);

	m_log = new RffeTerminal(pal, card);
	m_log->setObjectName(QStringLiteral("rffeFlashLog"));
	m_log->setMinimumHeight(160);
	// The recovered sheet styles the log's own vertical scrollbar under this name.
	m_log->verticalScrollBar()->setObjectName(QStringLiteral("rffeFlashLogBar"));
	body->addWidget(m_log, 1);

	m_logBar = new QProgressBar(card);
	m_logBar->setObjectName(QStringLiteral("rffeActivity"));
	m_logBar->setRange(0, 0); // indeterminate activity
	m_logBar->setTextVisible(false);
	m_logBar->setMaximumHeight(4);
	body->addWidget(m_logBar);

	m_warn = new QLabel(card);
	m_warn->setObjectName(QStringLiteral("rffeWarn"));
	m_warn->setWordWrap(true);
	m_warn->setStyleSheet(QStringLiteral("color:%1;").arg(pal.ledDanger.name()));
	m_warn->hide();
	body->addWidget(m_warn);

	root->addWidget(card, 1);

	auto *buttons = new QHBoxLayout();
	buttons->addStretch(1);
	m_cancel = new RffeButton(tr("Cancel"), RffeButton::Normal, this);
	connect(m_cancel, &QPushButton::clicked, this, &RffeFlashDialog::onCancel);
	buttons->addWidget(m_cancel);
	root->addLayout(buttons);

	if (m_flasher) {
		connect(m_flasher, &RffeFlasher::outputLine, this, &RffeFlashDialog::onOutput);
		connect(m_flasher, &RffeFlasher::progress, this, &RffeFlashDialog::onProgress);
		connect(m_flasher, &RffeFlasher::finished, this, &RffeFlashDialog::onFinished);
	}
}

void RffeFlashDialog::setStage(const QString &stage)
{
	if (stage.isEmpty()) return;
	m_stage->setText(stage);
}

void RffeFlashDialog::onOutput(const QString &line)
{
	m_log->appendLine(line);
	const QString stage = stageForLine(line);
	if (!stage.isEmpty()) setStage(stage);
}

void RffeFlashDialog::onProgress(int percent)
{
	if (percent < 0) return;
	m_progress->setValue(qBound(0, percent, 100));
}

void RffeFlashDialog::onFinished(bool ok, const QString &message)
{
	m_running = false;
	m_logBar->hide();
	m_cancel->setText(tr("Close"));
	if (ok) {
		setStage(tr("Firmware updated. The probe has restarted."));
		m_progress->setValue(100);
		m_log->appendOk(message);
	} else {
		setStage(message);
		m_warn->setText(message);
		m_warn->show();
		m_log->appendError(message);
	}
}

void RffeFlashDialog::onCancel()
{
	if (!m_running) {
		reject();
		return;
	}
	if (m_flasher) m_flasher->cancel();
	m_running = false;
	m_logBar->hide();
	m_cancel->setText(tr("Close"));
	setStage(tr("Update cancelled. Nothing was written."));
	m_log->appendWarn(tr("Update cancelled. Nothing was written."));
}

} // namespace rffe
} // namespace w2r
