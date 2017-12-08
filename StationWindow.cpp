/***************************************************************************
 *                                                                         *
 *   Copyright (C) 2017 by University of Illinois			   *
 *                                                                         *
 *   http://illinois.edu                                                   *
 *                                                                         *
 ***************************************************************************/

/**
 * @file    StationWindow.cpp
 *
 * A Station object organizes drones and senders, communicates with Vicon and
 * updates measurements for drones using a separate thread.
 *
 * A StationWindow is the main GUI.
 *
 * This is the View of the
 * Model(Station)-View(StationWindow)-Controller(StationController) pattern.
 *
 * @author  Bo Liu  <boliu1@illinois.edu>
 *
 */
#include "StationWindow.h"
#include "ui_StationWindow.h" // generated by ui form
#include <iostream>
#include <unistd.h>
#include <QFutureWatcher>
#include <QString>
#include <QMessageBox>
#include <QDebug>

StationWindow::StationWindow(std::unique_ptr<StationController> &controller, QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::StationWindow),
    controller(controller)
{
    ui->setupUi(this);
    setupConnections();
    initialize();
}

StationWindow::~StationWindow()
{
    delete ui;
}

void StationWindow::closeEvent (QCloseEvent *event)
{
    QMessageBox::StandardButton resBtn = QMessageBox::question( this, "ViconStation",
								tr("Are you sure?\n"),
								QMessageBox::Cancel | QMessageBox::No | QMessageBox::Yes,
								QMessageBox::Yes);
    if (resBtn != QMessageBox::Yes) {
	event->ignore();
    } else {
	event->accept();
	qApp->quit();
    }
}

void StationWindow::initialize()
{
    setOffline();
    ui->hostAddressLine->setText(controller->getHostAddress().toString());
    ui->hostPortLine->setText(QString::number(controller->getHostPort()));
    ui->viconGPSLine->setText(controller->getOriginGPS());
    setNorth("-x");
}

void StationWindow::setupConnections()
{
    connect(ui->actionConnect_Vicon, &QAction::triggered,
	    this, &StationWindow::launchViconStream);
    connect(ui->actionDisconnect_Vicon, &QAction::triggered,
	    this, &StationWindow::stopViconStream);
    connect(ui->actionQuit, &QAction::triggered,
	   this, &QApplication::quit);

    connect(ui->startMavLinkSenderButton, &QPushButton::released,
	    this, &StationWindow::startSenderHandler);
    connect(controller.get(), &StationController::droneNameAdded,
	    this, &StationWindow::addName);
    connect(controller.get(), &StationController::droneNameRemoved,
	    this, &StationWindow::removeName);
    connect(controller.get(), &StationController::viconConnected,
	    this, &StationWindow::setOnline);
    connect(controller.get(), &StationController::viconDisconnected,
	    this, &StationWindow::setOffline);
    connect(controller.get(), &StationController::dtUpdated,
	    this, &StationWindow::updatedt);
}

void StationWindow::launchViconStream()
{
    ui->hostAddressLine->setEnabled(false);
    ui->hostPortLine->setEnabled(false);
    ui->viconGPSLine->setEnabled(false);
    ui->NorthMapComboBox->setEnabled(false);

    controller->setHostAddress(ui->hostAddressLine->text());
    controller->setHostPort(ui->hostPortLine->text().toUShort());
    controller->setOriginGPS(ui->viconGPSLine->text());
    controller->setNorth(ui->NorthMapComboBox->currentText());
    controller->connectVicon();
}

void StationWindow::stopViconStream()
{
    controller->disconnectVicon();

    ui->hostAddressLine->setEnabled(true);
    ui->hostPortLine->setEnabled(true);
    ui->viconGPSLine->setEnabled(true);
    ui->NorthMapComboBox->setEnabled(true);
}

void StationWindow::addName(QString name)
{
    qDebug() << "adding drone to UI" << name;
    ui->listWidget->addItem(name);
}

void StationWindow::removeName(QString name)
{
    qDebug() << "removing drone from UI" << name;
    auto items = ui->listWidget->findItems(name, Qt::MatchCaseSensitive);
    for(auto& item : items)
    {
	ui->listWidget->removeItemWidget(item);
	delete item;
    }
}

void StationWindow::startSenderHandler()
{
    auto item = ui->listWidget->currentItem();
    if (!item) {
	QMessageBox msgBox;
	msgBox.setText("Please select an object first!");
	msgBox.exec();
	return;
    }

    QString name = item->text();
    qDebug() << "launch a MavLink sender: " << name;

    std::unique_ptr<Sender> sender { new Sender(name, controller->getStation()) };
    senders[name] = std::move(sender);
    std::unique_ptr<SenderController> senderController {new SenderController(senders[name]) };
    senderControllers[name] = std::move(senderController);
    std::unique_ptr<SenderWindow> w { new SenderWindow(name, senderControllers[name]) };
    senderWindows[name] = std::move(w);

    senderWindows[name]->show();

    qDebug() << "new senderWindow created.";
    qDebug() << "Station now has " << senders.size() << " sender windows.";

    connect(senderWindows[name].get(), &SenderWindow::closeSelf,
	    this, &StationWindow::senderWindowCloseHandler);
}

void StationWindow::updatedt(double dt)
{
    QString qdt("dt = ");
    qdt = qdt + QString::number(int(dt*1000)) + QString(" ms");
    ui->dtLabel->setText(qdt);
}

void StationWindow::senderWindowCloseHandler(const QString &name)
{
    auto it = senderWindows.find(name);
    if (it != std::end(senderWindows))
    {
	qDebug() << " sender window for: " << name << " closed.";
	senderWindows.erase(it);
	qDebug() << "Station has " << senderWindows.size() << " sender windows.";
    }

    auto it_c = senderControllers.find(name);
    senderControllers.erase(it_c);

    auto it_s = senders.find(name);
    senders.erase(it_s);
}

void StationWindow::setOnline()
{
    auto rate = QString::number(controller->getFrameRate());

    ui->status->setText(QString("ON LINE (") + rate + QString("Hz)"));
    setLabelColor(ui->status, Qt::darkGreen);
}

void StationWindow::setOffline()
{
    ui->status->setText("OFF LINE");
    setLabelColor(ui->status, Qt::red);
}

void StationWindow::setNorth(const QString &axis)
{
    auto index = ui->NorthMapComboBox->findText(axis);
    if (index != -1)
	ui->NorthMapComboBox->setCurrentIndex(index);
}

void StationWindow::setLabelColor(QLabel * label, QColor color)
{
    QPalette palette;
    palette.setColor(QPalette::Window, Qt::blue);
    palette.setColor(QPalette::WindowText, color);
    label->setPalette(palette);
}

