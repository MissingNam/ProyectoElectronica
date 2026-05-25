import sys, serial
import serial.tools.list_ports

from PyQt6.QtWidgets import (
    QApplication, QWidget, QLabel,
    QPushButton, QTextEdit, QLineEdit,
    QComboBox, QVBoxLayout, QHBoxLayout
)

from PyQt6.QtCore import QThread, pyqtSignal


class Window(QWidget):

    def __init__(self):
        super().__init__()

        self.arduino = None   # conexión serial futura

        self.setWindowTitle("Morse Link")
        self.resize(800,500)

        self.buildUI()
        self.loadPorts()


    def buildUI(self):

        self.stateLabel = QLabel()
        self.updateState(
            "DESCONECTADO"
        )

        self.portBox = QComboBox()
        self.refreshBtn = QPushButton("Actualizar")
        self.connectBtn = QPushButton("Conectar")

        self.logBox = QTextEdit()
        self.logBox.setReadOnly(True)

        self.sendField = QLineEdit()
        self.sendBtn = QPushButton("Enviar")
        self.sendBtn.clicked.connect(self.sendText)

        self.rxField = QTextEdit()
        self.rxField.setReadOnly(True)


        # eventos botones
        self.refreshBtn.clicked.connect(self.loadPorts)
        self.connectBtn.clicked.connect(self.connectArduino)


        top = QHBoxLayout()
        top.addWidget(self.portBox)
        top.addWidget(self.refreshBtn)
        top.addWidget(self.connectBtn)
        top.addStretch()
        top.addWidget(self.stateLabel)


        sendLayout = QHBoxLayout()
        sendLayout.addWidget(self.sendField)
        sendLayout.addWidget(self.sendBtn)


        right = QVBoxLayout()
        right.addWidget(QLabel("Texto a enviar"))
        right.addLayout(sendLayout)
        right.addWidget(QLabel("Texto recibido"))
        right.addWidget(self.rxField)


        middle = QHBoxLayout()
        middle.addWidget(self.logBox,2)
        middle.addLayout(right,1)


        layout = QVBoxLayout()
        layout.addLayout(top)
        layout.addLayout(middle)

        self.setLayout(layout)


    def loadPorts(self):

        self.portBox.clear()

        ports = serial.tools.list_ports.comports()

        for port in ports:
            self.portBox.addItem(port.device)

        self.log("Puertos actualizados")


    def connectArduino(self):

        try:

            port = self.portBox.currentText()

            self.arduino = serial.Serial(port,9600,timeout=1)
            self.serialThread = SerialThread(self.arduino)
            self.serialThread.message.connect(self.processMessage)
            self.serialThread.start()

            self.updateState("IDLE")

            self.log(
                f"Conectado a {port}"
            )

        except Exception as e:

            self.log(
                f"Error: {e}"
            )


    def log(self,msg):

        self.logBox.append(msg)

    def processMessage(self,msg):

        self.log(msg)
        parts = msg.split("|")
        if len(parts) != 2:
            return


        tipo = parts[0]
        data = parts[1]

        if tipo == "S":
            self.updateState(data)
        elif tipo == "R":
            actual = (self.rxField.toPlainText())
            self.rxField.setText(actual + data)
        elif tipo == "T":
            self.log(f"TX {data}")

    def sendText(self):
        if not self.arduino:
            return

        text = self.sendField.text()
        cmd = f"W|{text}0"

        self.arduino.write(cmd.encode())
        self.log(f"PC -> {cmd.strip()}")

    def updateState(self,state):
        colors = {
            "DESCONECTADO":"gray",
            "IDLE":"gray",
            "WRITING":"orange",
            "READING":"dodgerblue",
            "SENDING":"green",
            "ERROR":"red"
        }

        color = colors.get(state,"black")
        self.stateLabel.setText(f"● {state}")
        self.stateLabel.setStyleSheet(
            f"""
            font-weight:bold;
            color:{color};
            """
        )


class SerialThread(QThread):

    message = pyqtSignal(str)

    def __init__(self, arduino):
        super().__init__()
        self.arduino = arduino
        self.running = True


    def run(self):

        while self.running:

            try:

                if self.arduino.in_waiting:

                    msg = (
                        self.arduino
                        .readline()
                        .decode()
                        .strip()
                    )

                    self.message.emit(msg)

            except:
                pass


    def stop(self):
        self.running = False



app = QApplication(sys.argv)
window = Window()
window.show()

sys.exit(app.exec())