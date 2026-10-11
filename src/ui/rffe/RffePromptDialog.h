// RFFE prompt dialog: a single-line text prompt in a card (used for "New shield name for %1:").
#pragma once

#include "core/rffe/RffeTheme.h"

#include <QDialog>
#include <QString>

class QLineEdit;
class QLabel;

namespace w2r {
namespace rffe {

class RffePromptDialog : public QDialog {
	Q_OBJECT
  public:
	explicit RffePromptDialog(const Palette &pal, const QString &title, const QString &label,
	                          const QString &initial, QWidget *parent = nullptr);

	QString text() const;

	// Convenience: shows the prompt modally; returns true and fills *out when accepted.
	static bool getText(const Palette &pal, QWidget *parent, const QString &title,
	                    const QString &label, const QString &initial, QString *out);

  protected:
	void accept() override;

  private:
	QLineEdit *m_edit = nullptr;
};

} // namespace rffe
} // namespace w2r
