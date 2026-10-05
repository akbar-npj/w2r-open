#include "ui/PreferencesDialog.h"

#include "core/Settings.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

namespace w2r {

PreferencesDialog::PreferencesDialog(const QStringList &themeNames, QWidget *parent)
    : QDialog(parent)
{
	setWindowTitle(QStringLiteral("Preferences"));
	resize(560, 420);

	auto *root = new QVBoxLayout(this);

	// --- Library roots ---
	root->addWidget(new QLabel(QStringLiteral("<b>Library roots</b> (folders scanned for boardviews &amp; schematics)"), this));
	m_roots = new QListWidget(this);
	m_roots->addItems(Settings::libraryRoots());
	root->addWidget(m_roots, 1);

	auto *buttons = new QHBoxLayout();
	auto *add = new QPushButton(QStringLiteral("Add Folder..."), this);
	auto *remove = new QPushButton(QStringLiteral("Remove"), this);
	connect(add, &QPushButton::clicked, this, &PreferencesDialog::addRoot);
	connect(remove, &QPushButton::clicked, this, &PreferencesDialog::removeRoot);
	buttons->addWidget(add);
	buttons->addWidget(remove);
	buttons->addStretch(1);
	root->addLayout(buttons);

	// --- Appearance / view ---
	auto *form = new QFormLayout();
	m_theme = new QComboBox(this);
	m_theme->addItems(themeNames);
	const QString savedTheme = Settings::themeName();
	if (!savedTheme.isEmpty()) {
		const int idx = m_theme->findText(savedTheme);
		if (idx >= 0) m_theme->setCurrentIndex(idx);
	}
	form->addRow(QStringLiteral("Theme:"), m_theme);

	m_diode = new QCheckBox(QStringLiteral("Show diode readings on pads"), this);
	m_diode->setChecked(Settings::showDiodeReadings());
	form->addRow(QString(), m_diode);

	m_ratsnest = new QCheckBox(QStringLiteral("Show ratsnest for the selected net"), this);
	m_ratsnest->setChecked(Settings::showRatsnest());
	form->addRow(QString(), m_ratsnest);
	root->addLayout(form);

	auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	connect(bb, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
	root->addWidget(bb);
}

QStringList PreferencesDialog::libraryRoots() const
{
	QStringList out;
	for (int i = 0; i < m_roots->count(); ++i) out << m_roots->item(i)->text();
	return out;
}

QString PreferencesDialog::themeName() const { return m_theme->currentText(); }
bool PreferencesDialog::showDiodeReadings() const { return m_diode->isChecked(); }
bool PreferencesDialog::showRatsnest() const { return m_ratsnest->isChecked(); }

void PreferencesDialog::addRoot()
{
	const QString dir = QFileDialog::getExistingDirectory(this, QStringLiteral("Add Library Root"));
	if (dir.isEmpty()) return;
	for (int i = 0; i < m_roots->count(); ++i)
		if (m_roots->item(i)->text() == dir) return;
	m_roots->addItem(dir);
}

void PreferencesDialog::removeRoot()
{
	delete m_roots->currentItem();
}

} // namespace w2r
