/*
	Copyright 2010 Warzone 2100 Project

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

#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "MaterialDock.h"
#include "TransformDock.h"
#include "meshdock.h"
#include "ImportDialog.h"
#include "ExportDialog.h"
#include "TextureDialog.h"
#include "UVEditor.h"
#include "LightColorDock.h"

#include <fstream>

#include <QFileInfo>
#include <QFileDialog>
#include <QInputDialog>
#include <QColorDialog>
#include <QMessageBox>
#include <QDir>
#include <QStyle>

#include <QtDebug>
#include <QVariant>

#include "Pie.h"
#include "WZLight.h"

QString MainWindow::buildAppTitle()
{
	QString name(WMIT_APPNAME " " WMIT_VER_STR);

	if (m_modelinfo.m_currentFile.isEmpty())
		return name;

	if (!m_pathvert.isEmpty() && !m_pathvert.startsWith(":"))
	{
		QFileInfo vertNfo(m_pathvert);
		QFileInfo fragNfo(m_pathfrag);
		name.prepend(vertNfo.fileName() + "/" + fragNfo.fileName() + " - ");
	}

	QFileInfo fileNfo(m_modelinfo.m_currentFile);
	if (!fileNfo.baseName().isEmpty())
		name.prepend(fileNfo.baseName() + " - ");

	return name;
}

MainWindow::MainWindow(QWZM &model, QWidget *parent) : QMainWindow(parent),
	m_ui(new Ui::MainWindow),
	m_materialDock(new MaterialDock(this)),
	m_transformDock(new TransformDock(this)),
	m_meshDock(new MeshDock(this)),
	m_lightColorDock(new LightColorDock(lightCol0_custom, this)),
	m_textureDialog(new TextureDialog(this)),
	m_UVEditor(new UVEditor(this)),
	m_settings(new QSettings(this)),
	m_shaderSignalMapper(new QSignalMapper(this)),
	m_actionEnableUserShaders(nullptr),
	m_actionLocateUserShaders(nullptr),
	m_actionReloadUserShaders(nullptr),
	m_model(&model)
{
	m_ui->setupUi(this);

	m_pathImport = m_settings->value(WMIT_SETTINGS_IMPORTVAL, QDir::currentPath()).toString();
	m_pathExport = m_settings->value(WMIT_SETTINGS_EXPORTVAL, QDir::currentPath()).toString();

	m_materialDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
	m_materialDock->hide();

	m_transformDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
	m_transformDock->hide();

	m_meshDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
	m_meshDock->hide();

	m_lightColorDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
	m_lightColorDock->hide();

	m_UVEditor->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
	m_UVEditor->hide();

	addDockWidget(Qt::RightDockWidgetArea, m_materialDock, Qt::Horizontal);
	addDockWidget(Qt::RightDockWidgetArea, m_transformDock, Qt::Horizontal);
	addDockWidget(Qt::RightDockWidgetArea, m_meshDock, Qt::Horizontal);
	addDockWidget(Qt::RightDockWidgetArea, m_lightColorDock, Qt::Horizontal);
	addDockWidget(Qt::LeftDockWidgetArea, m_UVEditor, Qt::Horizontal);

	// UI is ready and now we can load window previous state (will do nothing if state wasn't saved).
	// 3DView specifics are loaded later on viewerInitialized event
	resize(QSettings().value("Window/size", size()).toSize());
	move(QSettings().value("Window/position", pos()).toPoint());
	restoreState(QSettings().value("Window/state", QByteArray()).toByteArray());

	loadLightColorSetting();

	m_ui->actionOpen->setIcon(QIcon::fromTheme("document-open", style()->standardIcon(QStyle::SP_DirOpenIcon)));
	m_ui->menuOpenRecent->setIcon(QIcon::fromTheme("document-open-recent"));
	m_ui->actionClearRecentFiles->setIcon(QIcon::fromTheme("edit-clear-list"));
	m_ui->actionSave->setIcon(QIcon::fromTheme("document-save", style()->standardIcon(QStyle::SP_DialogSaveButton)));
	m_ui->actionSaveAs->setIcon(QIcon::fromTheme("document-save-as"));
	m_ui->actionClose->setIcon(QIcon::fromTheme("window-close"));
	m_ui->actionExit->setIcon(QIcon::fromTheme("application-exit", style()->standardIcon(QStyle::SP_DialogCloseButton)));
	m_ui->actionAboutApplication->setIcon(QIcon::fromTheme("help-about"));

	connect(m_ui->centralWidget, SIGNAL(viewerInitialized()), this, SLOT(viewerInitialized()));
	connect(m_ui->menuFile, SIGNAL(aboutToShow()), this, SLOT(updateRecentFilesMenu()));
	connect(m_ui->actionOpen, SIGNAL(triggered()), this, SLOT(actionOpen()));
	connect(m_ui->menuOpenRecent, SIGNAL(triggered(QAction*)), this, SLOT(actionOpenRecent(QAction*)));
	connect(m_ui->actionClearRecentFiles, SIGNAL(triggered()), this, SLOT(actionClearRecentFiles()));
	connect(m_ui->actionClear_Missing_Files, SIGNAL(triggered()), this, SLOT(actionClearMissingFiles()));
	connect(m_ui->actionSave, SIGNAL(triggered()), this, SLOT(actionSave()));
	connect(m_ui->actionSaveAs, SIGNAL(triggered()), this, SLOT(actionSaveAs()));
	connect(m_ui->actionClose, SIGNAL(triggered()), this, SLOT(actionClose()));
	connect(m_ui->actionUVEditor, SIGNAL(toggled(bool)), m_UVEditor, SLOT(setVisible(bool)));
	connect(m_ui->actionSetupTextures, SIGNAL(triggered()), this, SLOT(actionSetupTextures()));
	connect(m_ui->actionAppendModel, SIGNAL(triggered()), this, SLOT(actionAppendModel()));
	connect(m_ui->actionImport_Animation, SIGNAL(triggered()), this, SLOT(actionImport_Animation()));
	connect(m_ui->actionImport_Connectors, SIGNAL(triggered()), this, SLOT(actionImport_Connectors()));
	connect(m_ui->actionShowAxes, SIGNAL(toggled(bool)), m_ui->centralWidget, SLOT(setAxisIsDrawn(bool)));
	connect(m_ui->actionShowGrid, SIGNAL(toggled(bool)), m_ui->centralWidget, SLOT(setGridIsDrawn(bool)));
	connect(m_ui->actionShowLightSource, SIGNAL(toggled(bool)), m_ui->centralWidget, SLOT(setDrawLightSource(bool)));
	connect(m_ui->actionLink_Light_Source_To_Camera, SIGNAL(toggled(bool)), m_ui->centralWidget, SLOT(setLinkLightToCamera(bool)));
	connect(m_ui->actionAnimate, SIGNAL(toggled(bool)), m_ui->centralWidget, SLOT(setAnimateState(bool)));
	connect(m_ui->actionEnable_Ecm_Effect, SIGNAL(toggled(bool)), this, SLOT(setEcmState(bool)));
	connect(m_ui->actionAboutQt, SIGNAL(triggered()), QApplication::instance(), SLOT(aboutQt()));
	connect(m_ui->actionSetTeamColor, SIGNAL(triggered()), this, SLOT(actionSetTeamColor()));

	/// Material dock
	m_materialDock->toggleViewAction()->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_M));
	m_ui->menuModel->insertAction(m_ui->menuModel->actions().value(0) ,m_materialDock->toggleViewAction());

	connect(m_materialDock, SIGNAL(materialChanged(WZMaterial)), this, SLOT(materialChangedFromUI(WZMaterial)));

	/// Transform dock
	m_transformDock->toggleViewAction()->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_T));
	m_ui->menuModel->insertAction(m_ui->menuModel->actions().value(0) ,m_transformDock->toggleViewAction());

	// transformations
	connect(m_transformDock, SIGNAL(scaleXYZChanged(double)), this, SLOT(scaleXYZChanged(double)));
	connect(m_transformDock, SIGNAL(scaleXChanged(double)), this, SLOT(scaleXChanged(double)));
	connect(m_transformDock, SIGNAL(scaleYChanged(double)), this, SLOT(scaleYChanged(double)));
	connect(m_transformDock, SIGNAL(scaleZChanged(double)), this, SLOT(scaleZChanged(double)));
	connect(m_transformDock, SIGNAL(reverseWindings()), this, SLOT(reverseWindings()));
	connect(m_transformDock, SIGNAL(flipNormals()), this, SLOT(flipNormals()));
	connect(m_transformDock, SIGNAL(applyTransformations()), m_model, SLOT(applyTransformations()));
	connect(m_transformDock, SIGNAL(changeActiveMesh(int)), m_model, SLOT(setActiveMesh(int)));
	connect(m_transformDock, SIGNAL(recalculateTB()), m_model, SLOT(slotRecalculateTB()));
	connect(m_transformDock, SIGNAL(removeMesh()), this, SLOT(removeMesh()));
	connect(m_transformDock, SIGNAL(mirrorAxis(int)), this, SLOT(mirrorAxis(int)));
	connect(m_transformDock, SIGNAL(centerMesh(int)), this, SLOT(centerMesh(int)));
	connect(m_model, SIGNAL(meshCountChanged(int,QStringList)), m_transformDock, SLOT(setMeshCount(int,QStringList)));

	/// Mesh dock
	m_meshDock->toggleViewAction()->setShortcut(QKeySequence(Qt::Key_M));
	m_ui->menuModel->insertAction(m_ui->menuModel->actions().value(0), m_meshDock->toggleViewAction());

	connect(m_meshDock, SIGNAL(connectorsWereUpdated()), this, SLOT(updateModelRender()));
	connect(m_model, SIGNAL(meshCountChanged(int,QStringList)), m_meshDock, SLOT(setMeshCount(int,QStringList)));

	// LightColor dock
	connect(m_lightColorDock, SIGNAL(colorsChanged()), this, SLOT(lightColorChangedFromUI()));
	connect(m_lightColorDock, SIGNAL(useCustomColorsChanged(bool)), this, SLOT(useCustomLightColorChangedFromUI(bool)));
	m_ui->menuView->insertAction(m_ui->actionSetTeamColor, m_lightColorDock->toggleViewAction());

	/// Reset state
	clear();
}

MainWindow::~MainWindow()
{
	delete m_ui;
}

void MainWindow::clear()
{
	m_model->clear();
	m_modelinfo.clear();

	setWindowTitle(buildAppTitle());

	doAfterModelWasLoaded(false);
}

void MainWindow::doAfterModelWasLoaded(const bool success)
{
	const bool hasAnim = m_model->hasAnimObject();

	m_ui->actionClose->setEnabled(success);
	m_ui->actionSave->setEnabled(success);
	m_ui->actionSaveAs->setEnabled(success);
	m_ui->actionSetupTextures->setEnabled(success);
	m_ui->actionAppendModel->setEnabled(success);
	m_ui->actionImport_Animation->setEnabled(success);

	// Disallow mirroring as it will mess-up animation
	m_transformDock->setMirrorState(success && !hasAnim);

	m_ui->actionShowModelCenter->setEnabled(!hasAnim);
}

bool MainWindow::openFile(const QString &filePath)
{
	if (filePath.isEmpty())
	{
		return false;
	}

	ModelInfo tmpinfo(m_modelinfo);
	WZM tmpmodel;

	if (loadModel(filePath, tmpmodel, tmpinfo))
	{
		QFileInfo modelFileNfo(filePath);

		m_modelinfo = tmpinfo;
		m_modelinfo.m_currentFile = modelFileNfo.absoluteFilePath();
		*m_model = tmpmodel;

		setWindowTitle(buildAppTitle());

		if (!fireTextureDialog(true))
		{
			clear();
			return false;
		}

		m_materialDock->setMaterial(m_model->getMaterial());

		doAfterModelWasLoaded();
	}

	return true;
}

bool MainWindow::guessModelTypeFromFilename(const QString& fname, wmit_filetype_t& type)
{
	const QString ext = fname.right(fname.size() - fname.lastIndexOf('.') - 1);

	if (ext.compare(QString("wzm"), Qt::CaseInsensitive) == 0)
	{
		type = WMIT_FT_WZM;
	}
	else if (ext.compare(QString("obj"), Qt::CaseInsensitive) == 0)
	{
		type = WMIT_FT_OBJ;
	}
	else if (ext.compare(QString("pie"), Qt::CaseInsensitive) == 0)
	{
		type = WMIT_FT_PIE;
	}
	else
	{
		return false;
	}

	return true;
}

bool MainWindow::saveModel(const WZM &model, const ModelInfo &info)
{
	std::ofstream out;
	out.open(info.m_saveAsFile.toLocal8Bit().constData());

	switch (info.m_save_type)
	{
	case WMIT_FT_WZM:
		model.write(out);
		break;
	case WMIT_FT_OBJ:
		model.exportToOBJ(out);
		break;
	default:
		Pie3Model p3 = model;

		if (info.m_save_type == WMIT_FT_PIE2)
		{
			Pie2Model p2 = p3;
			p2.write(out, &info.m_pieCaps);
		}
		else
		{
			p3.write(out, &info.m_pieCaps);
		}
	}

	out.close();

	return true;
}

void MainWindow::changeEvent(QEvent *event)
{
	QMainWindow::changeEvent(event);

	switch (event->type())
	{
	case QEvent::LanguageChange:
		m_ui->retranslateUi(this);
		break;
	default:
		break;
	}
}

void MainWindow::closeEvent(QCloseEvent *event)
{
	QSettings settings;

	settings.setValue("Window/size", size());
	settings.setValue("Window/position", pos());
	settings.setValue("Window/state", saveState());

	settings.setValue("3DView/ShowModelCenter", m_ui->actionShowModelCenter->isChecked());
	settings.setValue("3DView/ShowNormals", m_ui->actionShowNormals->isChecked());
	settings.setValue("3DView/ShowTangentAndBitangent", m_ui->actionShow_Tangent_And_Bitangent->isChecked());
	settings.setValue("3DView/ShowAxes", m_ui->actionShowAxes->isChecked());
	settings.setValue("3DView/ShowGrid", m_ui->actionShowGrid->isChecked());
	settings.setValue("3DView/ShowLightSource", m_ui->actionShowLightSource->isChecked());
	settings.setValue("3DView/LinkLightToCamera", m_ui->actionLink_Light_Source_To_Camera->isChecked());
	settings.setValue("3DView/EnableUserShaders", m_actionEnableUserShaders->isChecked());
	settings.setValue("3DView/Animate", m_ui->actionAnimate->isChecked());
	settings.setValue("3DView/EcmEffect", m_ui->actionEnable_Ecm_Effect->isChecked());
	settings.setValue("3DView/ShowConnectors", m_ui->actionShow_Connectors->isChecked());
	settings.setValue("3DView/ShaderTag", wz_shader_type_tag[getShaderType()]);

	saveLightColorSettings();

	event->accept();
}

bool MainWindow::loadModel(const QString& file, WZM& model, ModelInfo &info, bool nogui)
{
	wmit_filetype_t type;

	if (!guessModelTypeFromFilename(file, type))
	{
		printf("Could not guess model type from filename. Only formats PIE, WZM, and OBJ are supported.\n");
		return false;
	}

	info.m_read_type = type;

	bool read_success = false;
	std::ifstream f;
	ImportDialog* importDialog = nullptr;
	QSettings* settings = nullptr;

	f.open(file.toLocal8Bit(), std::ios::in | std::ios::binary);

	switch (type)
	{
	case WMIT_FT_WZM:
		read_success = model.read(f);
		break;
	case WMIT_FT_OBJ:
		if (!nogui)
		{
			importDialog = new ImportDialog();
			int result = importDialog->exec();
			delete importDialog;
			importDialog = nullptr;

			if (result != QDialog::Accepted)
			{
				return false;
			}
		}

		settings = new QSettings();

		read_success = model.importFromOBJ(f, settings->value(WMIT_SETTINGS_IMPORT_WELDER, true).toBool());
		break;
	case WMIT_FT_PIE:
	case WMIT_FT_PIE2:
		int pieversion = pieVersion(f);
		if (pieversion <= 2)
		{
			Pie2Model p2;
			read_success = p2.read(f);
			if (read_success)
			{
				Pie3Model p3(p2);
				info.m_pieCaps = p3.getCaps();
				model = WZM(p3);
			}
		}
		else // 3 or higher
		{
			Pie3Model p3;
			read_success = p3.read(f);
			if (read_success)
			{
				info.m_pieCaps = p3.getCaps();
				model = WZM(p3);
			}
		}
	}

	f.close();

	if (importDialog)
		delete importDialog;
	if (settings)
		delete settings;

	return read_success;
}

bool MainWindow::fireTextureDialog(const bool reinit)
{
	QMap<wzm_texture_type_t, QString> texmap;

	if (reinit)
	{
		m_model->getTexturesMap(texmap);
		m_textureDialog->setTexturesMap(texmap);
		m_textureDialog->createTextureIcons(m_pathImport, m_modelinfo.m_currentFile);
	}

	if (m_textureDialog->exec() == QDialog::Accepted)
	{
		m_model->clearTextureNames();
		m_model->clearGLRenderTextures();

		m_textureDialog->getTexturesFilepath(texmap);
		for (QMap<wzm_texture_type_t, QString>::const_iterator it = texmap.begin(); it != texmap.end(); ++it)
		{
			if (!it.value().isEmpty())
			{
				QFileInfo texFileNfo(it.value());
				m_model->loadGLRenderTexture(it.key(), texFileNfo.filePath());
				m_model->setTextureName(it.key(), texFileNfo.fileName().toStdString());
			}
		}

		return true;
	}

	return false;
}

void MainWindow::actionOpen()
{
	QString filePath;
	QFileDialog* fileDialog = new QFileDialog(this,
						  tr("Select File to open"),
						  m_pathImport,
						  tr("All Compatible (*.wzm *.pie *.obj);;"
						     "WZM models (*.wzm);;"
						     "PIE models (*.pie);;"
						     "OBJ files (*.obj)"));
	fileDialog->setFileMode(QFileDialog::ExistingFile);
	fileDialog->exec();

	if (fileDialog->result() == QDialog::Accepted)
	{
		filePath = fileDialog->selectedFiles().first();

		// refresh import working dir
		m_pathImport = fileDialog->directory().absolutePath();
		m_settings->setValue(WMIT_SETTINGS_IMPORTVAL, m_pathImport);

		PrependFileToRecentList(filePath);
	}

	delete fileDialog;
	fileDialog = nullptr;

	if (!filePath.isEmpty())
	{
		openFile(filePath);
		// else popup on fail?
	}
}

void MainWindow::actionOpenRecent(QAction *action)
{
	QString filename = action->data().toString();
	if (!filename.isEmpty())
	{
		PrependFileToRecentList(filename);
		openFile(filename);
	}
}

void MainWindow::actionClearRecentFiles()
{
	QSettings().remove("recentFiles");
}

void MainWindow::actionClearMissingFiles()
{
	QStringList recentFiles = QSettings().value("recentFiles").toStringList();

	recentFiles.removeDuplicates();

	for (auto& curFile: recentFiles)
	{
		QFileInfo fileInfo(curFile);
		if (!fileInfo.exists())
			recentFiles.removeAll(curFile);
	}

	QSettings().setValue("recentFiles", recentFiles);
}

void MainWindow::actionSave()
{
	m_modelinfo.prepareForSaveToSelf();

	if (m_modelinfo.m_currentFile.isEmpty())
	{
		actionSaveAs();
		return;
	}

	saveModel(*m_model, m_modelinfo);
}

void MainWindow::PrependFileToRecentList(const QString& filename)
{
	QFileInfo fileInfo(filename);

	QStringList recentFiles = QSettings().value("recentFiles").toStringList();
	recentFiles.removeAll(fileInfo.absoluteFilePath());
	recentFiles.prepend(fileInfo.absoluteFilePath());
	recentFiles = recentFiles.mid(0, 10);

	QSettings().setValue("recentFiles", recentFiles);
}

void MainWindow::actionSaveAs()
{
	ModelInfo tmpModelinfo(m_modelinfo);

	QStringList filters;
	filters << "PIE3 models (*.pie)" << "PIE2 models (*.pie)" << "WZM models (*.wzm)" << "OBJ files (*.obj)";

	QList<wmit_filetype_t> types;
	types << WMIT_FT_PIE << WMIT_FT_PIE2 << WMIT_FT_WZM << WMIT_FT_OBJ;

	QFileDialog* fDialog = new QFileDialog();

	fDialog->setFileMode(QFileDialog::AnyFile);
	fDialog->setAcceptMode(QFileDialog::AcceptSave);
	fDialog->setNameFilters(filters);
	fDialog->setWindowTitle(tr("Choose output file"));
	fDialog->setDirectory(m_pathExport);
	fDialog->exec();

	if (fDialog->result() != QDialog::Accepted)
	{
		return;
	}

	if (!filters.contains(fDialog->selectedNameFilter()))
	{
		return;
	}

	tmpModelinfo.m_save_type = types[filters.indexOf(fDialog->selectedNameFilter())];

	// refresh export working dir
	m_pathExport = fDialog->directory().absolutePath();
	m_settings->setValue(WMIT_SETTINGS_EXPORTVAL, m_pathExport);

	QFileInfo finfo(fDialog->selectedFiles().first());
	tmpModelinfo.m_saveAsFile = finfo.filePath();

	QPointer<ExportDialog> dlg;

	switch (tmpModelinfo.m_save_type)
	{
	case WMIT_FT_PIE:
	case WMIT_FT_PIE2:
		tmpModelinfo.defaultPieCapsIfNeeded();
		dlg = new PieExportDialog(tmpModelinfo.m_pieCaps, this);
		if (dlg->exec() == QDialog::Accepted)
		{
			tmpModelinfo.m_pieCaps = static_cast<PieExportDialog*>(dlg.data())->getCaps();
		}

		if (finfo.suffix().toLower() != "pie")
			tmpModelinfo.m_saveAsFile += ".pie";
		break;
	case WMIT_FT_OBJ:
		if (finfo.suffix().toLower() != "obj")
			tmpModelinfo.m_saveAsFile += ".obj";
		break;
	case WMIT_FT_WZM:
		if (finfo.suffix().toLower() != "wzm")
			tmpModelinfo.m_saveAsFile += ".wzm";
		break;
	}

	if (dlg && dlg->result() != QDialog::Accepted)
	{
		return;
	}

	m_modelinfo = tmpModelinfo;
	PrependFileToRecentList(m_modelinfo.m_saveAsFile);

	saveModel(*m_model, m_modelinfo);
}

bool MainWindow::reloadShader(wz_shader_type_t type, bool user_shader, QString *errMessage)
{
	m_pathvert.clear();
	m_pathfrag.clear();

	if (type == WZ_SHADER_NONE)
	{
		return true;
	}

	QString pathvert, pathfrag;
	if (user_shader)
	{
		pathvert = m_settings->value("shaders/user_vert_path").toString();
		pathfrag = m_settings->value("shaders/user_frag_path").toString();
	}
	else
	{
		switch (type)
		{
		case WZ_SHADER_WZ31:
			pathvert = WMIT_SHADER_WZ31_DEFPATH_VERT;
			pathfrag = WMIT_SHADER_WZ31_DEFPATH_FRAG;
			break;
		case WZ_SHADER_WZ32:
			pathvert = WMIT_SHADER_WZ32TC_DEFPATH_VERT;
			pathfrag = WMIT_SHADER_WZ32TC_DEFPATH_FRAG;
			break;
		case WZ_SHADER_WZ33:
			pathvert = WMIT_SHADER_WZ33TC_DEFPATH_VERT;
			pathfrag = WMIT_SHADER_WZ33TC_DEFPATH_FRAG;
			break;
		default:
			break;
		}
	}

	QFileInfo finfo(pathvert);
	if (finfo.exists())
	{
		finfo.setFile(pathfrag);
		if (finfo.exists())
		{
			if (m_ui->centralWidget->loadShader(type, pathvert, pathfrag, errMessage))
			{
				m_pathvert = pathvert;
				m_pathfrag = pathfrag;
				return true;
			}
		}
		else
			*errMessage = "Unable to find fragment shader!";
	}
	else
		*errMessage = "Unable to find vertex shader!";
	return false;
}

void MainWindow::viewerInitialized()
{
	// Only do init once
	disconnect(m_ui->centralWidget, SIGNAL(viewerInitialized()), this, SLOT(viewerInitialized()));

	m_ui->centralWidget->addToRenderList(m_model);
	m_ui->centralWidget->addToAnimateList(m_model);
	m_meshDock->setModel(m_model);

	m_actionEnableUserShaders = new QAction("Enable external shaders", this);
	m_actionEnableUserShaders->setCheckable(true);
	m_actionEnableUserShaders->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_E));
	connect(m_actionEnableUserShaders, SIGNAL(triggered(bool)), this, SLOT(actionEnableUserShaders(bool)));

	m_actionLocateUserShaders = new QAction("Locate external shaders...", this);
	m_actionLocateUserShaders->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_L));
	connect(m_actionLocateUserShaders, SIGNAL(triggered()), this, SLOT(actionLocateUserShaders()));

	m_actionReloadUserShaders = new QAction("Reload external shaders", this);
	m_actionReloadUserShaders->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_R));
	connect(m_actionReloadUserShaders, SIGNAL(triggered()), this, SLOT(actionReloadUserShader()));

	m_actionEnableTangentInShaders= new QAction("Enable tangents in shaders", this);
	m_actionEnableTangentInShaders->setCheckable(true);
	m_actionEnableTangentInShaders->setShortcut(QKeySequence(Qt::SHIFT + Qt::Key_T));
	connect(m_actionEnableTangentInShaders, SIGNAL(triggered(bool)), m_model, SLOT(setEnableTangentsInShaders(bool)));

	m_shaderGroup = new QActionGroup(this);

	for (int i = WZ_SHADER__FIRST; i < WZ_SHADER__LAST; ++i)
	{
		QString shadername = QWZM::shaderTypeToString(static_cast<wz_shader_type_t>(i));

		QAction* shaderAct = new QAction(shadername, this);

		m_shaderSignalMapper->setMapping(shaderAct, i);
		shaderAct->setActionGroup(m_shaderGroup);

		if (i < 9) // FIXME
			shaderAct->setShortcut(QKeySequence(tr("Ctrl+%1").arg(i+1)));
		shaderAct->setCheckable(true);

		reloadShader(static_cast<wz_shader_type_t>(i), false);

		connect(shaderAct, SIGNAL(triggered()), m_shaderSignalMapper, SLOT(map()));
	}

	connect(m_shaderSignalMapper, SIGNAL(mapped(int)), this, SLOT(shaderAction(int)));

	QMenu* rendererMenu = new QMenu(this);
	rendererMenu->addActions(m_shaderGroup->actions());

	// other user shader related stuff
	rendererMenu->addSeparator();
	rendererMenu->addAction(m_actionEnableUserShaders);
	rendererMenu->addAction(m_actionLocateUserShaders);
	rendererMenu->addAction(m_actionReloadUserShaders);
	rendererMenu->addAction(m_actionEnableTangentInShaders);

	m_ui->actionRenderer->setMenu(rendererMenu);

	connect(m_ui->actionShowModelCenter, SIGNAL(triggered(bool)),
		m_model, SLOT(setDrawCenterPointFlag(bool)));
	connect(m_ui->actionShowNormals, SIGNAL(triggered(bool)),
		m_model, SLOT(setDrawNormalsFlag(bool)));
	connect(m_ui->actionShowNormals, SIGNAL(triggered(bool)),
		m_ui->actionShow_Tangent_And_Bitangent, SLOT(setEnabled(bool)));
	connect(m_ui->actionShow_Tangent_And_Bitangent, SIGNAL(triggered(bool)),
		m_model, SLOT(setDrawTangentAndBitangentFlag(bool)));
	connect(m_ui->actionShow_Connectors, SIGNAL(triggered(bool)),
		m_model, SLOT(setDrawConnectors(bool)));

	/// Load previous state
	m_ui->actionShowModelCenter->setChecked(m_settings->value("3DView/ShowModelCenter", false).toBool());
	m_model->setDrawCenterPointFlag(m_ui->actionShowModelCenter->isChecked());

	m_ui->actionShowNormals->setChecked(m_settings->value("3DView/ShowNormals", false).toBool());
	m_model->setDrawNormalsFlag(m_ui->actionShowNormals->isChecked());

	m_ui->actionShow_Connectors->setChecked(m_settings->value("3DView/ShowConnectors", false).toBool());
	m_model->setDrawConnectors(m_ui->actionShow_Connectors->isChecked());

	m_ui->actionShow_Tangent_And_Bitangent->setChecked(m_settings->value("3DView/ShowTangentAndBitangent", false).toBool());
	m_model->setDrawTangentAndBitangentFlag(m_ui->actionShow_Tangent_And_Bitangent->isChecked());
	m_ui->actionShow_Tangent_And_Bitangent->setEnabled(m_ui->actionShowNormals->isChecked());

	m_ui->actionShowAxes->setChecked(m_settings->value("3DView/ShowAxes", true).toBool());
	m_ui->actionShowGrid->setChecked(m_settings->value("3DView/ShowGrid", true).toBool());

	m_ui->actionShowLightSource->setChecked(m_settings->value("3DView/ShowLightSource", true).toBool());
	m_ui->actionLink_Light_Source_To_Camera->setChecked(m_settings->value("3DView/LinkLightToCamera", true).toBool());

	m_actionEnableUserShaders->setChecked(m_settings->value("3DView/EnableUserShaders", false).toBool());
	m_ui->actionAnimate->setChecked(m_settings->value("3DView/Animate", true).toBool());

	m_ui->actionEnable_Ecm_Effect->setChecked(m_settings->value("3DView/EcmEffect", false).toBool());

	actionEnableUserShaders(m_actionEnableUserShaders->isChecked());

	m_actionEnableTangentInShaders->setChecked(m_model->getEnableTangentsInShaders());

	// Default to latest
	int shaderTag = m_settings->value("3DView/ShaderTag", wz_shader_type_tag[WZ_SHADER__LATEST]).toInt();
	int shaderActIdx = -1;
	for (int i = WZ_SHADER__FIRST; i < WZ_SHADER__LAST; ++i)
	{
		if (shaderTag == wz_shader_type_tag[i])
			shaderActIdx = i;
	}
	// Select any previous selection
	if (shaderActIdx >= 0)
	{
		if (m_shaderGroup->actions().at(shaderActIdx)->isEnabled())
			m_shaderGroup->actions().at(shaderActIdx)->activate(QAction::Trigger);
		else
			shaderActIdx = -1;
	}

	// Otherwise use old approach
	if (shaderActIdx < 0)
	{
		for (int i = m_shaderGroup->actions().size() - 1; i >= 0; --i)
		{
			if (m_shaderGroup->actions().at(i)->isEnabled())
			{
				m_shaderGroup->actions().at(i)->activate(QAction::Trigger);
				break;
			}
		}
	}

	m_lightColorDock->refreshColorUI();
	m_lightColorDock->useCustomColors(isUsingCustomLightColor());
}

void MainWindow::shaderAction(int type)
{
	QString errMessage;
	bool useUserShader = false;
	wz_shader_type_t stype = static_cast<wz_shader_type_t>(type);

	if (m_actionEnableUserShaders->isChecked())
	{
		useUserShader = reloadShader(stype, true, &errMessage);
		if (!useUserShader)
		{
			QMessageBox::warning(this, "External shaders error",
				"Unable to load external shaders, so please ensure that they are correct and hit reload!"\
				"\nNOTE: Model might temporarily go into stealth mode due to this error...\n\n" +
				errMessage);
		}
	}

	// Handle light
	switch (static_cast<wz_shader_type_t>(type))
	{
	case WZ_SHADER_WZ33:
		switchLightToWzVer(LIGHT_WZ33, true);
		break;
	default:
		switchLightToWzVer(LIGHT_WZ32, true);
	}

	if (!useUserShader)
		reloadShader(stype, false);

	if (static_cast<wz_shader_type_t>(type) != WZ_SHADER_NONE)
	{

		if (!m_model->setActiveShader(static_cast<wz_shader_type_t>(type)))
		{
		    QMessageBox::warning(this, "Shaders error",
		       "Unable to activate requested shaders!");
		}
	}
	else
	{
		m_model->disableShaders();
	}
	updateModelRender();

	setWindowTitle(buildAppTitle());
}

void MainWindow::setEcmState(bool checked)
{
	m_model->setEcmState(checked);
}

void MainWindow::scaleXYZChanged(double val)
{
	m_model->setScaleXYZ(val);
	updateModelRender();
}

void MainWindow::scaleXChanged(double val)
{
	m_model->setScaleX(val);
	updateModelRender();
}

void MainWindow::scaleYChanged(double val)
{
	m_model->setScaleY(val);
	updateModelRender();
}

void MainWindow::scaleZChanged(double val)
{
	m_model->setScaleZ(val);
	updateModelRender();
}

void MainWindow::reverseWindings()
{
	m_model->reverseWinding(m_model->getActiveMesh());
	updateModelRender();
}

void MainWindow::flipNormals()
{
	m_model->flipNormals(m_model->getActiveMesh());
	updateModelRender();
}

void MainWindow::mirrorAxis(int axis)
{
	m_model->slotMirrorAxis(axis);
	updateModelRender();
}

void MainWindow::removeMesh()
{
	m_model->slotRemoveActiveMesh();
	updateModelRender();
}

void MainWindow::centerMesh(int axis)
{
	m_model->center(m_model->getActiveMesh(), axis);
	updateModelRender();
}

void MainWindow::materialChangedFromUI(const WZMaterial &mat)
{
	m_model->setMaterial(mat);
	updateModelRender();
}

void MainWindow::lightColorChangedFromUI()
{
	if (switchLightToCustomIfNeeded())
		updateModelRender();
}

void MainWindow::useCustomLightColorChangedFromUI(bool use)
{
	setUseCustomLightColor(use);
}

void MainWindow::actionReloadUserShader()
{
	wz_shader_type_t type = getShaderType();
	shaderAction(type);
}

void MainWindow::actionClose()
{
	clear();
}

void MainWindow::actionSetupTextures()
{
	fireTextureDialog();
}

void MainWindow::actionAppendModel()
{
	QString filePath;
	QFileDialog* fileDialog = new QFileDialog(this,
						  tr("Select file to append"),
						  m_pathImport,
						  tr("All Compatible (*.wzm *.pie *.obj);;"
						     "WZM models (*.wzm);;"
						     "PIE models (*.pie);;"
						     "OBJ files (*.obj)"));
	fileDialog->setFileMode(QFileDialog::ExistingFile);
	fileDialog->exec();

	if (fileDialog->result() == QDialog::Accepted)
	{
		filePath = fileDialog->selectedFiles().first();
	}
	delete fileDialog;
	fileDialog = nullptr;

	if (!filePath.isEmpty())
	{
		ModelInfo newinfo;
		WZM newmodel;

		if (loadModel(filePath, newmodel, newinfo))
		{
			for (int i = 0; i < newmodel.meshes(); ++i)
			{
				m_model->addMesh(newmodel.getMesh(i));
			}

			doAfterModelWasLoaded();
		}
	}
}

void MainWindow::actionTakeScreenshot()
{
    m_ui->centralWidget->saveSnapshot(false);
}

void MainWindow::actionSetTeamColor()
{
    QColor newColor = QColorDialog::getColor(m_model->getTCMaskColor(), this, "Select new TeamColor");
    if (newColor.isValid())
        m_model->setTCMaskColor(newColor);
}

void MainWindow::actionEnableUserShaders(bool checked)
{
	m_actionLocateUserShaders->setEnabled(checked);
	m_actionReloadUserShaders->setEnabled(checked);

	// if goes off, then reload shader
	//if (!checked)
		actionReloadUserShader();
}

void MainWindow::actionImport_Animation()
{
	if (m_model->meshes() == 0)
	    return;

	QString anim_path = QFileDialog::getOpenFileName(this, "Locate animation file", m_pathImport,
							 "PIE animation (*.ani);;Any file (*.*)");
	if (anim_path.isEmpty())
	    return;

	int meshIdx = -1;
	{
		QStringList items = m_model->getMeshNames();

		QString item = QInputDialog::getItem(this, tr("Select mesh for animation import"), "", items, 0, false);
		if (!item.isEmpty())
			meshIdx = items.indexOf(item);
	}

	if (meshIdx < 0)
		return;

	auto &mesh = m_model->getMesh(meshIdx);

	ApieAnimObject pieAnim;
	if (pieAnim.readStandaloneAniFile(anim_path.toLocal8Bit()))
	{
		mesh.importPieAnimation(pieAnim);
		// Might disable some anim-unfriendly actions
		doAfterModelWasLoaded(true);
	}
}

void MainWindow::actionImport_Connectors()
{
	if (m_model->meshes() == 0)
	    return;

	QString conn_path = QFileDialog::getOpenFileName(this, "Locate source file", m_pathImport,
							 "PIE models (*.pie);;Any file (*.*)");
	if (conn_path.isEmpty())
	    return;


	ModelInfo newinfo;
	WZM newmodel;

	if (!loadModel(conn_path, newmodel, newinfo))
		return;

	bool need_ask_about_replacement = true;
	bool replace_current_ones = false;

	int maxmeshes = std::max(newmodel.meshes(), m_model->meshes());
	for (int i = 0; i < maxmeshes; ++i)
	{
		const Mesh& src_mesh = newmodel.getMesh(i);
		if (!src_mesh.connectors())
			continue;

		Mesh& tgt_mesh = m_model->getMesh(i);
		if (tgt_mesh.connectors())
		{
			if (need_ask_about_replacement)
			{
				need_ask_about_replacement = false;

				QMessageBox::StandardButton reply;
				reply = QMessageBox::question(this, "",
					tr("Do you want to replace any existing connectors?"));
				if (reply == QMessageBox::Yes)
					replace_current_ones = true;
			}

			if (!replace_current_ones)
				continue;
		}

		tgt_mesh.replaceConnectors(src_mesh);
	}

	// Update related view
	updateConnectorsView();
	// And notify model info about new connectors
	m_modelinfo.m_pieCaps.set(PIE_OPT_DIRECTIVES::podCONNECTORS);
}

void MainWindow::actionLocateUserShaders()
{
    QString vert_path = QFileDialog::getOpenFileName(this, "Locate vertex shader",
                                                     m_settings->value("shaders/user_vert_path", "").toString(),
                                                     "Vertex shaders (*.vert);;Any file (*.*)");
    if (vert_path.isEmpty())
        return;
    QString frag_path = QFileDialog::getOpenFileName(this, "Locate fragment shader",
                                                     m_settings->value("shaders/user_frag_path", "").toString(),
                                                     "Fragment shaders (*.frag);;Any file (*.*)");
    if (frag_path.isEmpty())
        return;

    m_settings->setValue("shaders/user_vert_path", vert_path);
    m_settings->setValue("shaders/user_frag_path", frag_path);

    // and execute
    actionReloadUserShader();
}

void MainWindow::updateRecentFilesMenu()
{
	QStringList recentFiles = QSettings().value("recentFiles").toStringList();

	int fileCnt = recentFiles.count();
	for (int i = 0; i < 10; ++i)
	{
		m_ui->menuOpenRecent->actions().at(i)->setVisible(i < fileCnt);
		if (i < fileCnt)
		{
			QFileInfo fileInfo(recentFiles.at(i));

			QString text = QString("%1. %2 (%3)").arg(i + 1).arg(fileInfo.fileName()).arg(recentFiles.at(i));
			m_ui->menuOpenRecent->actions().at(i)->setText(text);
			m_ui->menuOpenRecent->actions().at(i)->setData(recentFiles.at(i));
		}
	}

	m_ui->menuOpenRecent->setEnabled(fileCnt);
}

void MainWindow::updateModelRender()
{
	m_ui->centralWidget->update();
}

void MainWindow::updateConnectorsView()
{
	m_meshDock->resetConnectorViewModel();
}
