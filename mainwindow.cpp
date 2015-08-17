#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "dllib.h"
#include <QDate>
#include <QTime>
#include <QDebug>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent),
  ui(new Ui::MainWindow),
  m_cmdHelper(new cmdHelper::cmdHelper),
  m_cmdHistory(new cmdHistory::cmdHistory) {
  ui->setupUi(this);
  // configure GUI widgets
  ui->actionDisconnect->setVisible(false);
  // configure autocomplete
  ui->lineEdit->setCompleter(m_cmdHelper->m_cmdCompleter);
  // catch command events
  ui->lineEdit->installEventFilter(this);
  // default text before connection is established
  ui->lineEdit->setPlaceholderText("Press ⌘K to establish a connection.");
  // disable tab focus policy
  ui->plainTextEdit->setFocusPolicy(Qt::NoFocus);
  // create the discovery agent
  m_discoveryAgent = new DiscoveryAgent();
  Q_CHECK_PTR(m_discoveryAgent);
  connect(m_discoveryAgent, SIGNAL(signalPMUDiscovered(PMU*)), this, SLOT(slotPMUDiscovered(PMU*)));
}

MainWindow::~MainWindow() {
  delete ui;
}

QString MainWindow::sendPmuCommand(QString cmd) {
  QString response;
  // figure out the length NOT including the space
  int len = cmd.length();
  int i = cmd.indexOf(' ');
  if (i != -1) {
    len = cmd.left(i).length();
  }
  DLResult ret = m_pmuUSB->issueCommand(cmd, response, len);
  if (ret != DLLIB_SUCCESS) {
    // parse error response
    response = m_cmdHelper->m_errorResponses.value(response);
  }
  return response;
}

bool MainWindow::eventFilter(QObject *target, QEvent *event) {
  QString timestamp;
  QString cmdRequest;
  QString cmdResponse;
  struct pmu * pmu;
  if (event->type() == QEvent::KeyPress) {
    QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
    switch (keyEvent->key()) {
    case Qt::Key_Return:
    case Qt::Key_Enter:
      // ignore requests until a connection is established
      if (ui->actionUseFTDICable && (m_pmuUSB == NULL)) {
        ui->lineEdit->clear();
        break;
      }
      // enter a new command
      cmdRequest = ui->lineEdit->text().simplified();
      if (cmdRequest.isEmpty()) {
        break;
      }
      // append new command to history
      m_cmdHistory->append(cmdRequest);
      // find the associated helper entry
      pmu = m_cmdHelper->m_cmdTable[cmdRequest];
      // send the command
      if (pmu != NULL) {
        cmdResponse = sendPmuCommand(pmu->cmd);
        if (pmu->parser != NULL) {
          cmdResponse = pmu->parser(cmdResponse);
        }
      } else {
        cmdResponse = sendPmuCommand(cmdRequest);
      }
      // print results
      ui->lineEdit->clear();
      if (ui->actionShow_Timestamp->isChecked()) {
        timestamp = QDate::currentDate().toString(Qt::ISODate) + " " + QTime::currentTime().toString(Qt::ISODate);
        cmdRequest = timestamp + " > " + cmdRequest;
      } else {
        cmdRequest = " > " + cmdRequest;
      }
      ui->plainTextEdit->appendPlainText(cmdRequest);
      ui->plainTextEdit->appendPlainText(cmdResponse);
      break;
    case Qt::Key_Tab:
      if (ui->lineEdit->cursorPosition() != m_cmdHelper->getCurrentCompletionLength()) {
        // accept current completion
        ui->lineEdit->end(false);
      } else {
        // next completion
        ui->lineEdit->setText(m_cmdHelper->getNextCompletion());
      }
      break;
    case Qt::Key_Up:
      // scroll back through command history
      ui->lineEdit->setText(m_cmdHistory->scrollBack());
      break;
    case Qt::Key_Down:
      // scroll forward through command history
      ui->lineEdit->setText(m_cmdHistory->scrollForward());
      break;
    case Qt::Key_Left:
      // SHIFT + LEFT clears the command line
      if (keyEvent->modifiers() & Qt::ShiftModifier) {
        ui->lineEdit->clear();
      }
      break;
    case Qt::Key_Home:
      // move cursor to start of line
      ui->lineEdit->home(false);
      break;
    case Qt::Key_End:
      // move cursor to end of line
      ui->lineEdit->end(false);
      break;
    default:
      break;
    }
  }
  return QObject::eventFilter(target, event); 
}

void MainWindow::on_actionUseFTDICable_triggered() {
  ui->actionUseTelegesisAdapter->setChecked(false);
}

void MainWindow::on_actionUseTelegesisAdapter_triggered() {
  ui->actionUseFTDICable->setChecked(false);
}

void MainWindow::on_actionConnect_triggered() {
  // establish a connection
  if (ui->actionUseFTDICable) {
    m_discoveryAgent->clearLists();
    do {
      m_discoveryAgent->discoverPMU_usbs();
      MSLEEP(500);
      QApplication::processEvents();
    }
    while (m_discoveryAgent->m_pmuList.isEmpty());
  } else if (ui->actionUseTelegesisAdapter) {
    // todo
  }
}

void MainWindow::on_actionDisconnect_triggered() {
  ui->actionDisconnect->setVisible(false);
  ui->actionConnect->setVisible(true);
  ui->actionUseFTDICable->setDisabled(false);
  ui->actionUseTelegesisAdapter->setDisabled(false);
  ui->actionConfigure->setDisabled(false);
  ui->lineEdit->setPlaceholderText("Press ⌘K to establish a connection.");
}

void MainWindow::on_actionConfigure_triggered() {
  qDebug() << "configure all the things!";
}

void MainWindow::on_actionClear_Output_triggered() {
  ui->plainTextEdit->clear();
}

void MainWindow::slotPMUDiscovered(PMU* pmu) {
  //DLDebug(100, DL_FUNC_INFO) << "Found pmu via USB: " << pmu->getEESNStr();
  m_pmuUSB = qobject_cast<PMU_USB*>(pmu);
  if (!m_pmuUSB) {
    //DLDebug(1, DL_FUNC_INFO) << "BUG! Converting PMU to PMU_USB failed.";
    return;
  }
  //report(tr("Fixture %1 connected.").arg(m_pmuUSB->getEESNStr()));
  // update controls
  ui->actionConnect->setVisible(false);
  ui->actionDisconnect->setVisible(true);
  ui->actionUseFTDICable->setDisabled(true);
  ui->actionUseTelegesisAdapter->setDisabled(true);
  ui->actionConfigure->setDisabled(true);
  ui->lineEdit->setPlaceholderText("Type a command here. Terminate by pressing ENTER.");
}
