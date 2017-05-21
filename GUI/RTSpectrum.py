# -*- coding: utf-8 -*-
"""
Created on Tue May 02 15:31:04 2017

@author: Minh Nhuong, refer to Spectrogram by Tony Dicola
http://learn.adafruit.com/fft-fun-with-fourier-transforms/
"""
import sys
import matplotlib
matplotlib.use('Qt5Agg')
from matplotlib.animation import FuncAnimation
from matplotlib.backends.backend_qt5agg import FigureCanvasQTAgg as FigureCanvas
from matplotlib.figure import Figure
from matplotlib import pyplot as plt
from matplotlib import style
import numpy as np

import serial
import serial.tools.list_ports 

from PyQt5.QtGui import QIcon, QIntValidator
from PyQt5.QtWidgets import (QMainWindow, QMessageBox, QPushButton, QMenu, QInputDialog, QWidget, QVBoxLayout, QLabel)
from PyQt5.QtWidgets import (QHBoxLayout, QComboBox, QGroupBox, QGridLayout, QApplication, QLineEdit)
from PyQt5 import QtCore


BAUDRATE = 115200


class Spectrum(FigureCanvas):
    def __init__(self, window):
        
        self.window = window
        self.sample = 128
        self.fftSize = 128
        self.sampleRate = 100000
        self.magnitude = np.zeros((self.fftSize))
        self.magnitude[50] = 10
        self.x_limit = self.sampleRate/2
        self.y_limit = 250
        self.magnitudes = np.zeros(int(self.fftSize))
        
        np.seterr(all ='ignore')
        self.fig = plt.figure(figsize=(12,4 ), dpi =100)
        #self.histAx = self.fig.add_subplot(111)
        self.histAx = plt.axes (xlim = (0,self.fftSize/2),ylim = (0,self.y_limit))
        self.histAx.set_title('Group 8 - Spectrum Analyzer')
        self.histAx.set_ylabel('Magnitude')
        self.histAx.set_xlabel('Frequency (Hz)')
        #self.mainPlot, =  self.histAx.plot([], [])
        self.mainPlot = self.histAx.bar(np.arange(int(self.fftSize/2)), np.zeros(int(self.fftSize/2)), width= 1, linewidth= 1.0, facecolor = 'blue')
        #self.histAx.grid()
        #self.histAx.hold(False)
        self.updateGraph()
        super(Spectrum, self).__init__(self.fig)
        self.ani = FuncAnimation(self.fig , self._update, frames= 1, interval = 1, blit = True)

    def updateFFTsize(self, fftSize):
        self.fftSize = fftSize          

    def updateSampleRate(self, sampleRate):
    	self.sampleRate = sampleRate

    def update_xlimit(self, x_limit):
    	self.x_limit = x_limit
    	self.histAx.set_xlim(0,np.floor(self.x_limit/1000))
    	self.ani.event_source.stop()
    	self.updateGraph()
    	self.ani = FuncAnimation(self.fig , self._update, frames= 200, interval = 1, blit = True)
    def update_ylimit(self, y_limit):
    	self.y_limit = y_limit
    	self.histAx.set_ylim(0, self.y_limit)

    def get_fftsize(self):
    	return self.fftSize

    def get_samplerate(self):
    	return self.sampleRate

    def updateGraph(self):

        ticks=  (np.linspace(0, self.fftSize/2, 11))
        labels = ['%d' % i for i in np.linspace(0, self.x_limit, 11)]
        self.histAx.set_xticks(ticks)
        self.histAx.set_xticklabels(labels)

    def _update(self, n):
        mags = self.window.getMagnitudes()
        if type(mags) == list:
            #print (*mags)
            a = sum(mags)/float(128)
            print ("%f" %a)
            mags = mags[0:int(self.fftSize/2)]

            for bin, mag in zip(self.mainPlot, mags):
                bin.set_height(mag)
            return self.mainPlot
        else:
            return ()

class MainWindow(QMainWindow):
    def __init__(self, devices):
        super(MainWindow, self).__init__()
        self.devices = devices          #list device connection
        self.openDevice = None
        main = QWidget()
        main.setLayout(self._setupMainLayout())
        self.setCentralWidget(main)
        self.setGeometry(50, 100, 1024, 550)
        self.setWindowTitle('FreeRTSpec v0.1 - Minh Nhuong')
        self.setWindowIcon(QIcon('orbz_fire.ico'))
        self.file_menu = QMenu('&File', self)
        self.file_menu.addAction('&Quit', self.fileQuit,
                QtCore.Qt.CTRL + QtCore.Qt.Key_Q)
        self.menuBar().addMenu(self.file_menu)
        self.help_menu = QMenu('&Help', self)
        self.menuBar().addSeparator()
        self.menuBar().addMenu(self.help_menu)
        self.help_menu.addAction('&About', self.about_clicked)
        self.show()
    
    def closeEvent(self, event):
        if self.isDeviceOpen():
            self.openDevice.close()
        event.accept()
        
    def getMagnitudes(self):
        try:
            if self.isDeviceOpen():
                return self.openDevice.get_magnitude()

        except IOError as er:
            self._ErrorCommunication(er)
        
    
    def isDeviceOpen(self):
        return self.openDevice != None
    
    
    def _ErrorCommunication(self, error):
        
        mb = QMessageBox()
        mb.setText('Connection Error! ( %s)' % error)
        mb.exec_()
        self._closeDevice()
        
    def _setupMainLayout(self):
        self.spectr = Spectrum(self)

        info = QGroupBox('Information')
        info.setLayout(QGridLayout())
        info.layout().addWidget(QLabel('<b> FreeRTSpec v0.1</b>' ))
        info.layout().addWidget(QLabel('__@nhuongmh__'))
        info.layout().addWidget(QLabel('Embedded Programming Course 2017'))
        infovbox = QHBoxLayout()
        infovbox.addWidget(info)
        infovbox.addStretch(1)

        port = QHBoxLayout()
        deviceList = QComboBox()
        for device in sorted(self.devices,key =  lambda a : a.get_name()):
            deviceList.addItem(device.get_name(), userData=device)

        deviceButton = QPushButton('Open')
        deviceButton.clicked.connect(self._deviceButton)
        refreshbutton = QPushButton('Refresh')
        refreshbutton.clicked.connect(self.refreshport)
        self.deviceList = deviceList
        self.deviceButton = deviceButton
        port.addWidget(QLabel('Port:'))
        port.addWidget(deviceList)
        port.addWidget(refreshbutton)
        port.addWidget(deviceButton)

        baudrateStatus = QHBoxLayout()
        baudrateBut = QPushButton('Change')
        baudrateBut.clicked.connect(self._setBaudrate)
        baudrateNum = QLabel('%d' %115200) 
        baudrateStatus.addWidget(QLabel('Baudrate: '))
        baudrateStatus.addWidget(baudrateNum)
        baudrateStatus.addWidget(baudrateBut)
        self.baudrateNum = baudrateNum

        serialStuff = QVBoxLayout()
        serialStuff.addLayout(port)
        serialStuff.addLayout(baudrateStatus)
        #serialStuff.addStretch(1)


        fftsizeStatus = QHBoxLayout()
        setfftBut = QPushButton('Change')
        setfftBut.clicked.connect(self._setfftSize)
        fftsizeNum = QLabel('%d points' % self.spectr.get_fftsize())
        fftsizeStatus.addWidget(QLabel('FFT Size: '))
        fftsizeStatus.addWidget(fftsizeNum)
        fftsizeStatus.addWidget(setfftBut)
        self.fftsizeNum = fftsizeNum

        samplerateStatus = QHBoxLayout()
        setsprateBut = QPushButton('Change')
        setsprateBut.clicked.connect(self._setSampleRate)
        samplerateNum = QLabel('%d sps' % self.spectr.get_samplerate())
        samplerateStatus.addWidget(QLabel('Sample Rate: '))
        samplerateStatus.addWidget(samplerateNum)
        samplerateStatus.addWidget(setsprateBut)
        self.samplerateNum = samplerateNum

        serialStuff.addLayout(fftsizeStatus)
        serialStuff.addLayout(samplerateStatus)
        serialStuff.addStretch(1)


        set_xlimit = QHBoxLayout()
        self.set_xlimitNum = '10000'
        set_xlimitBut = QPushButton('Change')
        set_xlimitBut.clicked.connect(self.set_xlimitFunc)
        self.set_xlimitBox = QLineEdit()
        self.set_xlimitBox.setValidator(QIntValidator())
        self.set_xlimitBox.setText(self.set_xlimitNum)
        self.set_xlimitBox.resize(60,20)
        set_xlimit.addWidget(QLabel('Set X limit'))
        set_xlimit.addWidget(self.set_xlimitBox)
        set_xlimit.addWidget(set_xlimitBut)

        set_ylimit = QHBoxLayout()
        self.set_ylimitNum = '200'
        set_ylimitBut = QPushButton('Change')
        set_ylimitBut.clicked.connect(self.set_ylimitFunc)
        self.set_ylimitBox = QLineEdit()
        self.set_ylimitBox.setValidator(QIntValidator())
        self.set_ylimitBox.setText(self.set_ylimitNum)
        self.set_ylimitBox.resize(60,20)
        set_ylimit.addWidget(QLabel('Set Y limit'))
        set_ylimit.addWidget(self.set_ylimitBox)
        set_ylimit.addWidget(set_ylimitBut)

        set_limit = QVBoxLayout()
        set_limit.addLayout(set_xlimit)
        set_limit.addLayout(set_ylimit)
        set_limit.addStretch(1)

        leftmenu = QHBoxLayout()
        leftmenu.addLayout(serialStuff)
        leftmenu.addLayout(set_limit)
        leftmenu.addSpacing(10)

        bottom = QGridLayout()
        bottom.addLayout(leftmenu,0,0)
        bottom.addLayout(infovbox,0,1)
        bottom.setHorizontalSpacing(350)
        #bottom.addStretch(1)
        layout = QVBoxLayout()
        layout.addWidget(self.spectr)
        layout.addLayout(bottom)

        return layout
        
    def _deviceButton(self):
        if not self.isDeviceOpen():
            self._openDevice()
        else:
            self._closeDevice()
            
    def about_clicked(self):
       QMessageBox.about(self, "About", 
      """ FreeRTSpec v0.1 is a software developed by <b>Minh Nhuong</b> for Embedded Programming Course base on various sources """ )
        
    def _openDevice(self):
        try:
            device = self.deviceList.itemData(self.deviceList.currentIndex())
            if device != None:
            	device.open()
            	self.openDevice = device
            	self.deviceButton.setText('Close')
            
        except IOError as e:
            self._ErrorCommunication(e)
            

    def _closeDevice(self):
        try:
            if self.isDeviceOpen():
                self.openDevice.close()
                self.openDevice = None
                
            self.deviceButton.setText('Open')
            
        except IOError as e:
            self._ErrorCommunication(e)

    def fileQuit(self):
        self.close()

    def refreshport(self):
    	devices = enumerate_devices()
    	MainWindow(devices)

    def _setBaudrate(self):
    	QMessageBox.information(self, 'Change Baudrate', 'Sorry! this feature is not available right now.')

    def _setfftSize(self):
        fftsize, ok = QInputDialog.getInt(self, 'Set FFT Size', ' Enter new FFT Size' )
        if ok:
            self.spectr.updateFFTsize(fftsize)
            self.fftsizeNum.setText('%d points' %fftsize)
    
    
    def _setSampleRate(self):
        samplerate, ok = QInputDialog.getInt(self,'Set Sample Rate', ' Enter new Sample Rate')
        if ok:
            self.spectr.updateSampleRate(samplerate)
            self.samplerateNum.setText('%d sps' %samplerate)
            self.spectr.updateGraph()
    def set_xlimitFunc(self):
    	self.set_xlimitNum = self.set_xlimitBox.text()
    	self.spectr.update_xlimit(int(self.set_xlimitNum))
    def set_ylimitFunc(self):
    	self.set_ylimitNum = self.set_ylimitBox.text()
    	self.spectr.update_ylimit(int(self.set_ylimitNum))

class SerialPortDevice:
    def __init__(self, path, name):
        self.path = path
        self.name = name
        self.baudrate = BAUDRATE
        self.previous = np.zeros(128)
    def get_name(self):
        return self.name
        
    def get_magnitude(self):
        while 1:
            mag = int(ord(self.port.read()))
            if mag == 255:
                print ("%s" %'ok')
                mags_array =  [float(ord(self.port.read())) for i in range(128)]
                if (sum(mags_array)/128)<55 :
                    self.previous = mags_array
                    return mags_array
                else:
                    print ("%s" % 'mag error')
                    return [i-1 for i in self.previous]
            else:
                return self.previous
                print ("%s" % 'request error')

                    
    def open(self):
        self.port = serial.Serial(self.path, baudrate = self.baudrate, bytesize = serial.EIGHTBITS,
         	parity= serial.PARITY_NONE, stopbits = serial.STOPBITS_ONE,timeout= 5 )
        
    def close(self):
        self.port.close()

    def _readline(self):
        value = self.port.readline()
        if value == None or value =='':
            raise IOError('Error! _ Timeout.')
        print ("%s" %value)
        return value


def enumerate_devices():
    
    return [SerialPortDevice(port[0], port[1]) for port in serial.tools.list_ports.comports() if port[2] != 'n/a']



if __name__ == '__main__':
    app = QApplication(sys.argv)
    devices = enumerate_devices()
    aw = MainWindow(devices)
    sys.exit(app.exec_())
