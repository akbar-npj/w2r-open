#include "ui/ThemeEditorDialog.h"

#include <QColorDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

namespace w2r {

namespace {
// Paints a colour swatch onto a button.
void applySwatch(QPushButton *button, const QColor &color)
{
	button->setText(color.name(QColor::HexArgb));
	button->setStyleSheet(QStringLiteral("background-color: %1; color: %2;")
	                          .arg(color.name(QColor::HexArgb),
	                               color.lightness() > 128 ? QStringLiteral("#000000")
	                                                       : QStringLiteral("#ffffff")));
}
} // namespace

ThemeEditorDialog::ThemeEditorDialog(const Theme &theme, QWidget *parent)
    : QDialog(parent), m_theme(theme)
{
	setWindowTitle(tr("Theme Editor"));

	auto *outer = new QVBoxLayout(this);

	m_nameEdit = new QLineEdit(m_theme.name, this);
	auto *nameForm = new QFormLayout;
	nameForm->addRow(tr("Name"), m_nameEdit);
	outer->addLayout(nameForm);

	// Colour rows live in a scroll area because there are many (incl. 16 layers).
	auto *content = new QWidget;
	auto *form = new QFormLayout(content);
	form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

	addColorRow(form, tr("Background"), [this] { return m_theme.background; },
	            [this](const QColor &c) { m_theme.background = c; });
	addColorRow(form, tr("Board fill"), [this] { return m_theme.boardFill; },
	            [this](const QColor &c) { m_theme.boardFill = c; });
	addColorRow(form, tr("Outline"), [this] { return m_theme.outline; },
	            [this](const QColor &c) { m_theme.outline = c; });
	addColorRow(form, tr("Part outline"), [this] { return m_theme.partOutline; },
	            [this](const QColor &c) { m_theme.partOutline = c; });
	addColorRow(form, tr("Component text"), [this] { return m_theme.componentNameText; },
	            [this](const QColor &c) { m_theme.componentNameText = c; });
	addColorRow(form, tr("Pin"), [this] { return m_theme.pin; },
	            [this](const QColor &c) { m_theme.pin = c; });
	addColorRow(form, tr("Pin outline"), [this] { return m_theme.pinOutline; },
	            [this](const QColor &c) { m_theme.pinOutline = c; });
	addColorRow(form, tr("Pin text"), [this] { return m_theme.pinText; },
	            [this](const QColor &c) { m_theme.pinText = c; });
	addColorRow(form, tr("Ground pin"), [this] { return m_theme.groundPin; },
	            [this](const QColor &c) { m_theme.groundPin = c; });
	addColorRow(form, tr("NC pin"), [this] { return m_theme.ncPin; },
	            [this](const QColor &c) { m_theme.ncPin = c; });
	addColorRow(form, tr("Net text"), [this] { return m_theme.netText; },
	            [this](const QColor &c) { m_theme.netText = c; });
	addColorRow(form, tr("Silkscreen"), [this] { return m_theme.silkscreen; },
	            [this](const QColor &c) { m_theme.silkscreen = c; });
	addColorRow(form, tr("Selected pin"), [this] { return m_theme.selectedPin; },
	            [this](const QColor &c) { m_theme.selectedPin = c; });
	addColorRow(form, tr("Same-net pin"), [this] { return m_theme.sameNetPin; },
	            [this](const QColor &c) { m_theme.sameNetPin = c; });
	addColorRow(form, tr("Ratsnest"), [this] { return m_theme.ratsnest; },
	            [this](const QColor &c) { m_theme.ratsnest = c; });
	addColorRow(form, tr("Part highlight fill"), [this] { return m_theme.partHighlightFill; },
	            [this](const QColor &c) { m_theme.partHighlightFill = c; });
	addColorRow(form, tr("Part highlight border"), [this] { return m_theme.partHighlightBorder; },
	            [this](const QColor &c) { m_theme.partHighlightBorder = c; });
	for (int i = 0; i < m_theme.layers.size(); ++i) {
		addColorRow(form, tr("Layer %1").arg(i + 1), [this, i] { return m_theme.layers[i]; },
		            [this, i](const QColor &c) { m_theme.layers[i] = c; });
	}

	auto *scroll = new QScrollArea(this);
	scroll->setWidgetResizable(true);
	scroll->setWidget(content);
	outer->addWidget(scroll, 1);

	auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
	outer->addWidget(buttons);

	resize(420, 640);
}

void ThemeEditorDialog::addColorRow(QFormLayout *form, const QString &label,
                                    const std::function<QColor()> &get,
                                    const std::function<void(const QColor &)> &set)
{
	auto *button = new QPushButton(this);
	applySwatch(button, get());
	connect(button, &QPushButton::clicked, this, [this, get, set] {
		const QColor chosen = QColorDialog::getColor(get(), this, tr("Select colour"),
		                                             QColorDialog::ShowAlphaChannel);
		if (!chosen.isValid()) return;
		set(chosen);
		refreshSwatches();
		emit themeChanged(m_theme);
	});
	form->addRow(label, button);
	m_swatches.append(button);
	m_getters.append(get);
}

void ThemeEditorDialog::refreshSwatches()
{
	for (int i = 0; i < m_swatches.size(); ++i) applySwatch(m_swatches[i], m_getters[i]());
}

void ThemeEditorDialog::accept()
{
	m_theme.name = m_nameEdit->text().trimmed();
	if (m_theme.name.isEmpty()) m_theme.name = tr("Custom");
	QDialog::accept();
}

} // namespace w2r
