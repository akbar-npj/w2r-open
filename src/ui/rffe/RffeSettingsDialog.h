// RFFE settings dialog: the marking/animation options, in a card.
#pragma once

#include "core/rffe/RffeModel.h"
#include "core/rffe/RffeTheme.h"

#include <QDialog>

class QCheckBox;
class QSpinBox;

namespace w2r {
namespace rffe {

class RffeSettingsDialog : public QDialog {
	Q_OBJECT
  public:
	explicit RffeSettingsDialog(const Palette &pal, const MarkSettings &initial,
	                            QWidget *parent = nullptr);

	MarkSettings settings() const { return m_settings; }

  protected:
	void accept() override;

  private:
	MarkSettings m_settings;
	QSpinBox *m_blinkMs = nullptr;
	QSpinBox *m_holdMs = nullptr;
	QCheckBox *m_halo = nullptr;
	QCheckBox *m_esp32 = nullptr;
	QCheckBox *m_beep = nullptr;
};

} // namespace rffe
} // namespace w2r
