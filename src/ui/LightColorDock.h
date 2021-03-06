/*
	Copyright 2012 Warzone 2100 Project

	This file is part of WMIT.

	WMIT is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	WMIT is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with WMIT.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef LightColorDock_H
#define LightColorDock_H

#include "WZLight.h"

#include <QDockWidget>

namespace Ui
{
	class LightColorDock;
}

class LightColorDock : public QDockWidget
{
	Q_OBJECT

public:
	LightColorDock(light_cols_t& light_cols, QWidget *parent = nullptr);
	~LightColorDock();

protected:
	void changeEvent(QEvent *event);

signals:
	void colorsChanged();
	void useCustomColorsChanged(bool);

private slots:
	void colorsChangedOnWidget(const light_cols_t& light_cols);
	void useCustomColorsChangedOnWidget(int);

public slots:
	void refreshColorUI();
	void useCustomColors(const bool useState);

private:
	Ui::LightColorDock *m_ui;
	light_cols_t& m_light_cols;
};

#endif // LightColorDock_H
