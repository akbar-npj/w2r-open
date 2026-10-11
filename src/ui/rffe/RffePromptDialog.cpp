#include "ui/rffe/RffePromptDialog.h"

#include "ui/rffe/RffeWidgets.h"

#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace w2r {
namespace rffe {

RffePromptDialog::RffePromptDialog(const Palette &pal, const QString &title, const QString &label,
                                   const QString &initial, QWidget *parent)
    : QDialog(parent)
{
	setWindowTitle(title);
	resize(420, 200);
	// Panel sheet + the recovered dialog sheet (card, labels, line edit).
	setStyleSheet(pal.styleSheet() + pal.promptDialogStyleSheet());

	auto *root = new QVBoxLayout(this);
	root->setContentsMargins(12, 12, 12, 12);

	auto *card = new RffeCard(pal, QString(), this);
	card->setObjectName(QStringLiteral("rffePromptCard"));
	auto *body = card->body();

	auto *titleLabel = new QLabel(title, card);
	titleLabel->setObjectName(QStringLiteral("rffePromptTitle"));
	QFont tf = titleLabel->font();
	tf.setBold(true);
	titleLabel->setFont(tf);
	body->addWidget(titleLabel);

	auto *textLabel = new QLabel(label, card);
	textLabel->setObjectName(QStringLiteral("rffePromptText"));
	textLabel->setWordWrap(true);
	body->addWidget(textLabel);

	m_edit = new QLineEdit(initial, card);
	m_edit->setObjectName(QStringLiteral("rffePromptEdit"));
	m_edit->selectAll();
	body->addWidget(m_edit);

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

QString RffePromptDialog::text() const { return m_edit->text().trimmed(); }

void RffePromptDialog::accept()
{
	if (text().isEmpty()) return; // a name is required
	QDialog::accept();
}

bool RffePromptDialog::getText(const Palette &pal, QWidget *parent, const QString &title,
                               const QString &label, const QString &initial, QString *out)
{
	RffePromptDialog dlg(pal, title, label, initial, parent);
	if (dlg.exec() != QDialog::Accepted) return false;
	if (out) *out = dlg.text();
	return true;
}

} // namespace rffe
} // namespace w2r
