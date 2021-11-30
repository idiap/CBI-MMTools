/**
* Copyright (c) 2021 Idiap Research Institute, http://www.idiap.ch/
* Written by François Marelli <francois.marelli@idiap.ch>
* 
* This file is part of CBI-MMTools.
* 
* CBI-MMTools is free software: you can redistribute it and/or modify
* it under the terms of the 3-Clause BSD License.
* 
* CBI-MMTools is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* 3-Clause BSD License for more details.
* 
* You should have received a copy of the 3-Clause BSD License along
* with CBI-MMTools. If not, see https://opensource.org/licenses/BSD-3-Clause.
* 
* SPDX-License-Identifier: BSD-3-Clause 
*/

package ch.idiap.cbi;

import com.google.common.eventbus.Subscribe;

import java.awt.Color;
import java.awt.Font;
import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;
import java.io.File;
import java.io.IOException;
import java.nio.charset.Charset;
import java.nio.file.Files;
import java.nio.file.Paths;

import javax.swing.event.ChangeEvent;
import javax.swing.event.ChangeListener;
import javax.swing.filechooser.FileNameExtensionFilter;

import java.text.ParseException;
import javax.swing.JButton;
import javax.swing.JCheckBox;
import javax.swing.JSlider;
import javax.swing.JComboBox;
import javax.swing.JFileChooser;
import javax.swing.JFrame;
import javax.swing.JLabel;
import javax.swing.JOptionPane;
import javax.swing.JPanel;
import javax.swing.JTextField;
import javax.swing.JWindow;
import javax.swing.JFrame;
import javax.swing.SwingConstants;
import mmcorej.CMMCore;
import mmcorej.DeviceType;
import mmcorej.StrVector;
import net.miginfocom.swing.MigLayout;
import org.micromanager.Studio;
import org.micromanager.events.SystemConfigurationLoadedEvent;
import org.micromanager.events.ExposureChangedEvent;
import org.micromanager.events.internal.InternalShutdownCommencingEvent;
import org.micromanager.internal.utils.NumberUtils;
import org.micromanager.internal.utils.TextUtils;
import org.micromanager.internal.utils.WindowPositioning;

import org.jfree.chart.ChartPanel;
import org.jfree.chart.ChartFactory;
import org.jfree.chart.JFreeChart;
import org.jfree.chart.plot.PlotOrientation;
import org.jfree.data.xy.DefaultXYDataset;
import org.jfree.chart.renderer.xy.XYStepRenderer;
import org.jfree.chart.labels.StandardXYToolTipGenerator;
import java.awt.BasicStroke;

import mmcorej.org.json.JSONObject;
import mmcorej.org.json.JSONException;

public final class OpenLightControlFrame extends JFrame {

    private final Studio studio_;
    private final CMMCore core_;

    private final JFileChooser jsonChooser = new JFileChooser();;

    private boolean initialized_ = false;

    private final int frameXPos_ = 100;
    private final int frameYPos_ = 100;

    private int nFrames_;
    private int nSteps_;

    private JPanel errorPanel_;
    private JPanel masterPanel_;
    private JPanel graphPanel_;

    private JPanel[] outputPanels_;
    private JCheckBox[] channelEnable_;
    private JButton[] channelSetA_;
    private JButton[] channelSetD_;
    private JSlider[] channelAmplitude_;
    private JTextField[] channelAmplitudeF_;

    private JComboBox<String> deviceSelect_;
    private JComboBox<String> triggerSelect_;

    private JTextField frameLengthField_;
    private JTextField waitBeforeField_;
    private JTextField waitAfterField_;

    private JLabel exposureLabel_;
    private JLabel nStepsLabel_;
    private JLabel nFramesLabel_;

    private JCheckBox masterEnable;
    private JCheckBox manualEnable;
    private JCheckBox digitalEnable;
    private JCheckBox analogEnable;
    private JCheckBox loopEnable;

    private JButton acquireButton;
    private JButton snapButton;
    private JButton resetButton;

    private DefaultXYDataset digDataset;
    private DefaultXYDataset anaDataset;

    final private String[] deviceNames = { "OpenLightControl-Hub", "OpenLightControl-TriggerSelect", "OpenLightControl-Enable",
            "OpenLightControl-OutputP1", "OpenLightControl-OutputP2", "OpenLightControl-OutputO1", "OpenLightControl-OutputO2" };

    final private String[] channelNames = { "Output P1 (PWM)", "Output P2 (PWM)", "Output 1", "Output 2" };

    private String[] analogMods;
    private String[] digitalMods;

    private String[] deviceLabels;

    /**
     * Creates new form StageControlFrame
     *
     * @param gui the MM api
     */
    public OpenLightControlFrame(Studio gui) {
        studio_ = gui;
        core_ = studio_.getCMMCore();

        deviceLabels = new String[7];

        initComponents();

        super.setLocation(frameXPos_, frameYPos_);
        WindowPositioning.setUpLocationMemory(this, this.getClass(), null);
    }

    /**
     * Initialized GUI components based on current hardware configuration Can be
     * called at any time to adjust display (for instance after hardware
     * configuration change)
     */
    public final void initialize() {

        StrVector devices = core_.getLoadedDevicesOfType(DeviceType.HubDevice);

        ActionListener[] deviceActionListeners = removeListeners(deviceSelect_);

        deviceSelect_.removeAllItems();

        for (int i = 0; i < devices.size(); i++) {
            String label = devices.get(i);

            try {
                String deviceName = core_.getDeviceName(label);
                if (deviceName.equals(deviceNames[0])) {
                    deviceSelect_.addItem(label);
                }
            } catch (Exception ex) {
                studio_.logs().logError(ex, "Error when loading device " + label);
            }
        }

        setListeners(deviceSelect_, deviceActionListeners);

        boolean error = deviceSelect_.getItemCount() == 0;

        errorPanel_.setVisible(error);
        masterPanel_.setVisible(!error);
        graphPanel_.setVisible(!error);
        for (int channel = 0; channel < 4; channel++) {
            outputPanels_[channel].setVisible(!error);
        }

        if (!error) {
            if (deviceLabels[0] != null) {
                deviceSelect_.setSelectedItem(deviceLabels[0]);
            }
            updateDeviceProperties();
        }

        initialized_ = true;

        pack();
    }

    ActionListener[] removeListeners(JComboBox item) {
        ActionListener[] actionListeners = item.getActionListeners();
        for (ActionListener l : actionListeners) {
            item.removeActionListener(l);
        }
        return actionListeners;
    }

    void setListeners(JComboBox item, ActionListener[] listeners) {
        for (ActionListener l : listeners) {
            item.addActionListener(l);
        }
    }

    ChangeListener[] removeListeners(JSlider item) {
        ChangeListener[] actionListeners = item.getChangeListeners();
        for (ChangeListener l : actionListeners) {
            item.removeChangeListener(l);
        }
        return actionListeners;
    }

    void setListeners(JSlider item, ChangeListener[] listeners) {
        for (ChangeListener l : listeners) {
            item.addChangeListener(l);
        }
    }

    String getProperty(int device, String property) {
        String result = "";
        try {
            result = core_.getProperty(deviceLabels[device], property);
        } catch (Exception e) {
            studio_.logs().logError("Unknown error when getting property " + property);
        }
        return result;
    }

    boolean setProperty(int device, String property, double value) {
        try {
            core_.setProperty(deviceLabels[device], property, value);
            return true;
        } catch (Exception e) {
            updateDeviceProperties();
            return false;
        }
    }

    boolean setProperty(int device, String property, int value) {
        try {
            core_.setProperty(deviceLabels[device], property, value);
            return true;
        } catch (Exception e) {
            updateDeviceProperties();
            return false;
        }
    }

    boolean setProperty(int device, String property, String value) {
        try {
            core_.setProperty(deviceLabels[device], property, value);
            return true;
        } catch (Exception e) {
            updateDeviceProperties();
            return false;
        }
    }

    private void updateDeviceProperties() {
        deviceLabels[0] = (String) deviceSelect_.getSelectedItem();

        StrVector devices = core_.getLoadedDevices();

        for (int di = 0; di < devices.size(); di++) {
            try {
                if (core_.hasProperty(devices.get(di), "HubID")
                        && core_.getProperty(devices.get(di), "HubID").equals(deviceLabels[0])) {
                    for (int j = 0; j < deviceNames.length; j++) {
                        if (core_.getDeviceName(devices.get(di)).equals(deviceNames[j])) {
                            deviceLabels[j] = devices.get(di);
                            break;
                        }
                    }
                }
            } catch (Exception e) {
                studio_.logs().logError("Uknown error when loading devices");
            }
        }

        try {
            StrVector triggers = core_.getStateLabels(deviceLabels[1]);
            int state = core_.getState(deviceLabels[1]);

            ActionListener[] listeners = removeListeners(triggerSelect_);

            triggerSelect_.removeAllItems();

            for (int i = 0; i < triggers.size(); i++) {
                triggerSelect_.addItem(triggers.get(i));
            }
            triggerSelect_.setSelectedIndex(state);

            setListeners(triggerSelect_, listeners);

        } catch (Exception e) {
            studio_.logs().logError("Uknown error when loading triggers");
        }

        String property;

        property = getProperty(0, "FramePeriod");
        frameLengthField_.setText(property);

        property = getProperty(0, "NSteps");
        nSteps_ = Integer.parseInt(property);
        nStepsLabel_.setText(property);

        property = getProperty(0, "NFrames");
        nFrames_ = Integer.parseInt(property);
        nFramesLabel_.setText(property);

        property = getProperty(0, "DigitalModulation");
        int digMod = Integer.parseInt(property);
        digitalEnable.setSelected(digMod != 0);

        property = getProperty(0, "AnalogModulation");
        int anaMod = Integer.parseInt(property);
        analogEnable.setSelected(anaMod != 0);

        property = getProperty(0, "LoopFrame");
        int loop = Integer.parseInt(property);
        loopEnable.setSelected(loop != 0);

        property = getProperty(0, "AcquireFrames");
        int acq = Integer.parseInt(property);
        if (acq < 0) {
            acquireButton.setText("Stop");
            snapButton.setEnabled(false);
        } else {
            acquireButton.setText("Start");
            snapButton.setEnabled(true);
        }

        property = getProperty(0, "Exposure");
        double exposure = Double.parseDouble(property);
        if (exposure == 0.0) {
            manualEnable.setSelected(true);
            snapButton.setEnabled(false);
            exposureLabel_.setText("L");

            // Loop is default behaviour when manual exposure
            loopEnable.setSelected(true);
            loopEnable.setEnabled(false);
        } else {
            manualEnable.setSelected(false);
            if ((String) triggerSelect_.getSelectedItem() == "CamFireAll") {
                exposure += 10.0;
                property = Double.toString(exposure);
            }
            loopEnable.setEnabled(true);
            exposureLabel_.setText(property);
        }

        property = getProperty(2, "Enable");
        int enable = Integer.parseInt(property);
        masterEnable.setSelected(enable != 0);

        for (int channel = 0; channel < 4; channel++) {

            property = getProperty(3 + channel, "Gate");
            int gate = Integer.parseInt(property);
            channelEnable_[channel].setSelected(gate != 0);

            property = getProperty(3 + channel, "Amplitude");
            channelAmplitudeF_[channel].setText(property);
            int amp = Integer.parseInt(property);

            ChangeListener[] listeners = removeListeners(channelAmplitude_[channel]);
            channelAmplitude_[channel].setValue(amp);
            setListeners(channelAmplitude_[channel], listeners);

            property = getProperty(3 + channel, "ModulationA");
            analogMods[channel] = property;

            property = getProperty(3 + channel, "ModulationD");
            digitalMods[channel] = property;

        }

        updateGraph();
    }

    private void initComponents() {
        setTitle("OpenLightControl GUI");
        setLocationByPlatform(true);
        setResizable(false);
        setLayout(new MigLayout("fill, insets 10, gap 15, wrap 4", "[180!]20[180!]20[180!]20[180!]"));

        FileNameExtensionFilter jsonFilter = new FileNameExtensionFilter("JSON file", "json");

        jsonChooser.setFileFilter(jsonFilter);

        errorPanel_ = createErrorPanel();
        masterPanel_ = createMasterPanel();
        graphPanel_ = createGraphPanel();

        outputPanels_ = new JPanel[4];
        channelEnable_ = new JCheckBox[4];
        channelSetA_ = new JButton[4];
        channelSetD_ = new JButton[4];
        channelAmplitude_ = new JSlider[4];
        channelAmplitudeF_ = new JTextField[4];

        analogMods = new String[4];
        digitalMods = new String[4];

        for (int channel = 0; channel < 4; channel++) {
            outputPanels_[channel] = createOutputPanel(channel);
        }

        add(errorPanel_, "grow, hidemode 3, span");
        add(masterPanel_, "grow, hidemode 3");
        add(graphPanel_, "grow, span, hidemode 3");

        for (int channel = 0; channel < 4; channel++) {
            add(outputPanels_[channel], "grow, hidemode 3");
        }

    }

    private JPanel createMasterPanel() {
        final JFrame theWindow = this;
        JPanel innerPanel;

        JPanel result = new JPanel(new MigLayout("insets 2, gap 5, fill, flowy"));

        result.add(new JLabel("Master controls", JLabel.CENTER), "growx, gapbottom 10");
        deviceSelect_ = new JComboBox<String>();
        deviceSelect_.addActionListener(new ActionListener() {
            @Override
            public void actionPerformed(ActionEvent e) {

                updateDeviceProperties();
            }
        });

        result.add(deviceSelect_, "height 20!, growx");

        masterEnable = new JCheckBox("Enable");
        masterEnable.addActionListener(new ActionListener() {
            @Override
            public void actionPerformed(ActionEvent e) {
                if (masterEnable.isSelected()) {
                    setProperty(2, "Enable", 1);
                } else {
                    setProperty(2, "Enable", 0);
                }
            }
        });
        result.add(masterEnable, "height 30!, gapbottom 5, growx");

        innerPanel = new JPanel(new MigLayout("insets 0, gap 0, fillx, flowx"));
        triggerSelect_ = new JComboBox<String>();
        triggerSelect_.addActionListener(new ActionListener() {
            @Override
            public void actionPerformed(ActionEvent e) {
                int channel = triggerSelect_.getSelectedIndex();
                try {
                    core_.setState(deviceLabels[1], channel);
                } catch (Exception ex) {
                    studio_.logs().logError("Unknown error when selecting trigger");
                    updateDeviceProperties();
                }
            }
        });
        innerPanel.add(new JLabel("Trigger: ", JLabel.CENTER), "height 20!");
        innerPanel.add(triggerSelect_, "height 20!, growx, alignx right");
        result.add(innerPanel, "height 20!, growx");

        innerPanel = new JPanel(new MigLayout("insets 0, gap 0, fillx, flowx"));
        innerPanel.add(new JLabel("Exposure (ms): ", JLabel.CENTER), "height 20!");
        exposureLabel_ = new JLabel("", SwingConstants.RIGHT);
        innerPanel.add(exposureLabel_, "height 20!, growx, alignx right");
        result.add(innerPanel, "height 20!, growx");

        manualEnable = new JCheckBox("Long exposure");
        manualEnable.addActionListener(new ActionListener() {
            @Override
            public void actionPerformed(ActionEvent e) {
                if (manualEnable.isSelected()) {
                    if (!setProperty(0, "Exposure", 0.0)) {
                        return;
                    }
                    exposureLabel_.setText("L");

                    // Frame looping is default on long exposure
                    loopEnable.setSelected(true);
                    loopEnable.setEnabled(false);

                    snapButton.setEnabled(false);
                } else {
                    double exposure;
                    try {
                        exposure = core_.getExposure();

                        if ((String) triggerSelect_.getSelectedItem() == "CamFireAll") {
                            exposure -= 10.0;
                        }
                    } catch (Exception ex) {
                        exposure = -1.0;
                    }
                    if (!setProperty(0, "Exposure", exposure)) {
                        return;
                    }
                    exposureLabel_.setText(NumberUtils.doubleToDisplayString(exposure));
                    snapButton.setEnabled(true);

                    String property = getProperty(0, "LoopFrame");
                    int loop = Integer.parseInt(property);
                    loopEnable.setSelected(loop != 0);
                    loopEnable.setEnabled(true);
                }
            }
        });
        result.add(manualEnable, "height 20!, growx");

        innerPanel = new JPanel(new MigLayout("insets 0, gap 0, fillx, flowx"));
        innerPanel.add(new JLabel("# Frames: ", JLabel.CENTER), "height 20!");
        nFramesLabel_ = new JLabel("0", SwingConstants.RIGHT);
        innerPanel.add(nFramesLabel_, "height 20!, growx, alignx right, wrap");

        innerPanel.add(new JLabel("# Steps: ", JLabel.CENTER), "height 20!");
        nStepsLabel_ = new JLabel("0", SwingConstants.RIGHT);
        innerPanel.add(nStepsLabel_, "height 20!, growx, alignx right");
        result.add(innerPanel, "height 40!, growx");

        result.add(new JLabel("Modulation Control", JLabel.CENTER), "growx, gaptop 10");

        digitalEnable = new JCheckBox("Digital Modulation");
        digitalEnable.addActionListener(new ActionListener() {
            @Override
            public void actionPerformed(ActionEvent e) {
                if (digitalEnable.isSelected()) {
                    setProperty(0, "DigitalModulation", 1);
                } else {
                    setProperty(0, "DigitalModulation", 0);
                }
            }
        });
        result.add(digitalEnable, "height 20!, growx");

        analogEnable = new JCheckBox("Analog Modulation");
        analogEnable.addActionListener(new ActionListener() {
            @Override
            public void actionPerformed(ActionEvent e) {
                if (analogEnable.isSelected()) {
                    setProperty(0, "AnalogModulation", 1);
                } else {
                    setProperty(0, "AnalogModulation", 0);
                }
            }
        });
        result.add(analogEnable, "height 20!, growx");

        loopEnable = new JCheckBox("Periodic Frame Loop");
        loopEnable.addActionListener(new ActionListener() {
            @Override
            public void actionPerformed(ActionEvent e) {
                if (loopEnable.isSelected()) {
                    setProperty(0, "LoopFrame", 1);
                } else {
                    setProperty(0, "LoopFrame", 0);
                }
            }
        });
        result.add(loopEnable, "height 20!, growx");

        innerPanel = new JPanel(new MigLayout("insets 0, gap 0, fillx, flowx"));
        innerPanel.add(new JLabel("Loop period (ms): ", JLabel.CENTER), "height 20!");
        frameLengthField_ = new JTextField();
        frameLengthField_.addActionListener(new ActionListener() {
            @Override
            public void actionPerformed(ActionEvent e) {
                try {
                    double frameLength = NumberUtils.displayStringToDouble(frameLengthField_.getText());
                    setProperty(0, "FramePeriod", frameLength);
                } catch (ParseException ex) {
                    JOptionPane.showMessageDialog(theWindow, "Loop period is not a number");
                    updateDeviceProperties();
                }
            }
        });
        frameLengthField_.setHorizontalAlignment(SwingConstants.RIGHT);
        innerPanel.add(frameLengthField_, "height 20!, w 90!, alignx right");
        result.add(innerPanel, "height 20!, growx");

        innerPanel = new JPanel(new MigLayout("insets 0, gap 0, fillx, flowx"));
        innerPanel.add(new JLabel("Reset modulations: "), "h 20!");
        resetButton = new JButton("X");
        resetButton.addActionListener(new ActionListener() {
            @Override
            public void actionPerformed(ActionEvent e) {
                int result = JOptionPane.showConfirmDialog(theWindow, "Reset all modulations?", "Reset",
                        JOptionPane.YES_NO_OPTION);
                if (result == JOptionPane.YES_OPTION) {
                    if (!setProperty(0, "NFrames", 0) || !setProperty(0, "NSteps", 0)) {
                        return;
                    }

                    nSteps_ = 0;
                    nFrames_ = 0;
                    nStepsLabel_.setText(Integer.toString(nSteps_));
                    nFramesLabel_.setText(Integer.toString(nFrames_));

                    for (int channel = 0; channel < 4; channel++) {
                        digitalMods[channel] = "";
                        analogMods[channel] = "";
                    }

                    updateGraph();
                }
            }
        });
        innerPanel.add(resetButton, "h 20!, growx");
        result.add(innerPanel, "growx");

        snapButton = new JButton("Snap Image");
        snapButton.addActionListener(new ActionListener() {
            @Override
            public void actionPerformed(ActionEvent e) {
                setProperty(0, "AcquireFrames", 1);
            }
        });

        acquireButton = new JButton("Start");
        acquireButton.addActionListener(new ActionListener() {
            @Override
            public void actionPerformed(ActionEvent e) {
                if (acquireButton.getText() == "Start") {
                    if (!setProperty(0, "AcquireFrames", -1)) {
                        return;
                    }
                    acquireButton.setText("Stop");
                    snapButton.setEnabled(false);
                } else {
                    if (!setProperty(0, "AcquireFrames", 0)) {
                        return;
                    }
                    acquireButton.setText("Start");
                    if (!manualEnable.isSelected()) {
                        snapButton.setEnabled(true);
                    }
                }
            }
        });

        result.add(new JLabel("Manual Acquisition", JLabel.CENTER), "growx, gaptop 10");
        result.add(snapButton, "h 20!, growx");
        innerPanel = new JPanel(new MigLayout("insets 0, gap 0, fillx, flowx"));
        innerPanel.add(new JLabel("Continuous: "), "h 20!, growx");
        innerPanel.add(acquireButton, "h 20!, w 90!");
        result.add(innerPanel, "height 20!, growx");

        return result;
    }

    double[][] makeXYData(String sArray) {
        String[] sValues = sArray.split("-");
        double[][] dValues = new double[2][sValues.length + 1];

        for (int index = 0; index < sValues.length; index++) {
            try {
                dValues[1][index] = Double.parseDouble(sValues[index]);
                dValues[0][index] = (double) index / nSteps_;
            } catch (NumberFormatException e) {
                return new double[2][0];
            }
        }
        dValues[0][sValues.length] = (double) sValues.length / nSteps_;
        dValues[1][sValues.length] = dValues[1][sValues.length - 1];

        return dValues;
    }

    private void updateGraph() {
        for (int series = 0; series < digDataset.getSeriesCount(); series++) {
            boolean found = false;
            String seriesKey = (String) digDataset.getSeriesKey(series);
            for (int i = 0; i < 4; i++) {
                if (seriesKey.equals(channelNames[i])) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                digDataset.removeSeries(seriesKey);
            }
        }

        for (int channel = 0; channel < 4; channel++) {
            digDataset.addSeries(channelNames[channel], makeXYData(digitalMods[channel]));
            anaDataset.addSeries(channelNames[channel], makeXYData(analogMods[channel]));
        }
    }

    private JPanel createGraphPanel() {
        JPanel result = new JPanel(new MigLayout("insets 2, gap 5, fill, flowy"));
        result.setBackground(Color.white);

        ChartPanel chartPanel;
        JFreeChart lineChart;
        XYStepRenderer renderer = new XYStepRenderer();

        digDataset = new DefaultXYDataset();

        lineChart = ChartFactory.createXYLineChart("Digital Modulation", "Frames", "", digDataset,
                PlotOrientation.VERTICAL, true, true, false);
        lineChart.getXYPlot().setRenderer(renderer);

        chartPanel = new ChartPanel(lineChart);

        result.add(chartPanel, "h 240!, growx");

        anaDataset = new DefaultXYDataset();

        lineChart = ChartFactory.createXYLineChart("Analog Modulation", "Frames", "", anaDataset,
                PlotOrientation.VERTICAL, true, true, false);
        lineChart.getXYPlot().setRenderer(renderer);

        chartPanel = new ChartPanel(lineChart);

        result.add(chartPanel, "h 240!, growx");

        return result;
    }

    private JPanel createOutputPanel(int channel) {
        final JFrame theWindow = this;

        JPanel result = new JPanel(new MigLayout("insets 2, gap 5, fill, flowy"));
        result.add(new JLabel(channelNames[channel]), "h 30!, growx");

        channelEnable_[channel] = new JCheckBox("Enable");
        channelEnable_[channel].addActionListener(new ActionListener() {
            @Override
            public void actionPerformed(ActionEvent e) {
                if (channelEnable_[channel].isSelected()) {
                    setProperty(3 + channel, "Gate", 1);
                } else {
                    setProperty(3 + channel, "Gate", 0);
                }
            }
        });
        result.add(channelEnable_[channel], "h 20!, growx");

        JPanel innerPanel = new JPanel(new MigLayout("insets 0, gap 0, fillx, flowx"));
        channelAmplitude_[channel] = new JSlider(JSlider.HORIZONTAL, 0, 255, 0);
        channelAmplitude_[channel].addChangeListener(new ChangeListener() {
            @Override
            public synchronized void stateChanged(ChangeEvent e) {
                JSlider source = (JSlider) e.getSource();
                if (!source.getValueIsAdjusting()) {
                    int amplitude = channelAmplitude_[channel].getValue();
                    channelAmplitudeF_[channel].setText(Integer.toString(amplitude));

                    setProperty(3 + channel, "Amplitude", amplitude);
                }
            }
        });
        result.add(new JLabel("Amplitude"), "h 20!, growx, gaptop 5");
        innerPanel.add(channelAmplitude_[channel], "h 20!, w 135");

        channelAmplitudeF_[channel] = new JTextField("0");
        channelAmplitudeF_[channel].setHorizontalAlignment(SwingConstants.RIGHT);
        channelAmplitudeF_[channel].addActionListener(new ActionListener() {
            @Override
            public void actionPerformed(ActionEvent e) {
                try {
                    int amplitude = NumberUtils.displayStringToInt(channelAmplitudeF_[channel].getText());
                    channelAmplitude_[channel].setValue(amplitude);
                } catch (ParseException ex) {
                    JOptionPane.showMessageDialog(theWindow, "Amplitude is not a number");
                    updateDeviceProperties();
                }
            }
        });
        innerPanel.add(channelAmplitudeF_[channel], "h 20!, w 35!, gapleft 5");
        result.add(innerPanel);

        // result.add(new JLabel("Modulation"), "h 20!, growx, gaptop 5");
        channelSetD_[channel] = new JButton("Set Modulation");
        channelSetD_[channel].addActionListener(new ActionListener() {
            @Override
            public void actionPerformed(ActionEvent e) {

                int returnVal = jsonChooser.showOpenDialog(theWindow);
                if (returnVal == JFileChooser.APPROVE_OPTION) {
                    File file = jsonChooser.getSelectedFile();

                    try {
                        String content = new String(Files.readAllBytes(Paths.get(file.getAbsolutePath())));
                        try {
                            JSONObject json = new JSONObject(content);

                            int nframes = json.getInt("nframes");
                            int nsteps = json.getInt("nsteps");
                            String modulationD = json.getString("digital");
                            String modulationA = json.getString("analog");

                            if (nFrames_ != 0 && nSteps_ != 0 && (nframes != nFrames_ || nsteps != nSteps_)) {
                                JOptionPane.showMessageDialog(theWindow,
                                        "NFrames / NSteps do not match the current settings. Reset modulations first.");
                                return;
                            }

                            if (nSteps_ == 0 || nFrames_ == 0) {
                                if (!setProperty(0, "NFrames", nframes) || !setProperty(0, "NSteps", nsteps)) {
                                    return;
                                }

                                digitalEnable.setSelected(false);
                                analogEnable.setSelected(false);

                                nSteps_ = nsteps;
                                nFrames_ = nframes;
                                nStepsLabel_.setText(Integer.toString(nsteps));
                                nFramesLabel_.setText(Integer.toString(nframes));
                            }

                            if (setProperty(3 + channel, "ModulationA", modulationA)
                                    && setProperty(3 + channel, "ModulationD", modulationD)) {

                                digitalMods[channel] = modulationD;
                                analogMods[channel] = modulationA;

                                updateGraph();
                            }

                        } catch (JSONException exc) {
                            JOptionPane.showMessageDialog(theWindow, "Invalid JSON file.");
                            studio_.logs().logError("Invalid JSON file");
                        }
                    } catch (IOException ex) {
                        studio_.logs().logError("Unknown error when opening JSON file");
                    }
                }
            }
        });
        result.add(channelSetD_[channel], "h 20!, w 170!");

        return result;
    }

    private JPanel createErrorPanel() {
        JLabel noDriveLabel = new javax.swing.JLabel("No compatible device found.");
        noDriveLabel.setOpaque(true);

        JPanel panel = new JPanel(new MigLayout("fill"));
        panel.add(noDriveLabel, "align center, grow");

        return panel;
    }

    @Subscribe
    public void onSystemConfigurationLoaded(SystemConfigurationLoadedEvent event) {
        initialize();
    }

    @Subscribe
    public void onExposureChanged(ExposureChangedEvent event) {
        if (!manualEnable.isSelected()) {
            double exposure = event.getNewExposureTime();
            if ((String) triggerSelect_.getSelectedItem() == "CamFireAll") {
                exposure -= 10.0;
            }
            if (!setProperty(0, "Exposure", exposure)) {
                return;
            }
            exposureLabel_.setText(NumberUtils.doubleToDisplayString(exposure));
        }
    }

    @Subscribe
    public void onShutdownCommencing(InternalShutdownCommencingEvent event) {
        if (!event.isCanceled()) {
            this.dispose();
        }
    }

    @Override
    public void dispose() {

        super.dispose();
    }

}