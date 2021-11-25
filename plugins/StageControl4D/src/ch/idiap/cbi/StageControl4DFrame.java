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

import com.bulenkov.iconloader.IconLoader;
import com.google.common.eventbus.Subscribe;
import java.awt.Font;
import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;
import java.awt.geom.Point2D;
import java.text.ParseException;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import javax.swing.JButton;
import javax.swing.JComboBox;
import javax.swing.JFrame;
import javax.swing.JLabel;
import javax.swing.JOptionPane;
import javax.swing.JPanel;
import javax.swing.JTextField;
import javax.swing.JFrame;
import mmcorej.CMMCore;
import mmcorej.DeviceType;
import mmcorej.StrVector;
import net.miginfocom.swing.MigLayout;
import org.micromanager.Studio;
import org.micromanager.events.StagePositionChangedEvent;
import org.micromanager.events.SystemConfigurationLoadedEvent;
import org.micromanager.events.XYStagePositionChangedEvent;
import org.micromanager.events.internal.InternalShutdownCommencingEvent;
import org.micromanager.internal.utils.NumberUtils;
import org.micromanager.internal.utils.TextUtils;
import org.micromanager.internal.utils.WindowPositioning;

public final class StageControl4DFrame extends JFrame {

    private Double[] centers = new Double[] { 2000., 2000., 1200. };

    private final Studio studio_;
    private final CMMCore core_;
    private final OpticalRotation rotationManager;
    private CalibrationFrame calibrationFrame_;

    private boolean initialized_ = false;

    private final int frameXPos_ = 100;
    private final int frameYPos_ = 100;

    private final ExecutorService stageMotionExecutor_;

    private static final String[] XY_MOVEMENTS = new String[] { "SMALLMOVEMENT", "MEDIUMMOVEMENT", "LARGEMOVEMENT" };
    private static final String SMALLMOVEMENTZ = "SMALLMOVEMENTZ";
    private static final String MEDIUMMOVEMENTZ = "MEDIUMMOVEMENTZ";
    private static final String SMALLMOVEMENTR = "SMALLMOVEMENTR";
    private static final String MEDIUMMOVEMENTR = "MEDIUMMOVEMENTR";
    private static final String[] CENTERS = new String[] { "CENTERX", "CENTERY", "CENTERZ" };

    private JPanel errorPanel_;
    private JPanel xyPanel_;
    private JLabel xyPositionLabel_;
    private JPanel zPanel_;
    private JPanel rPanel_;
    private JPanel controlPanel_;
    private JComboBox zDriveSelect_;
    private JComboBox rDriveSelect_;
    private JLabel zPositionLabel_;
    private JLabel rPositionLabel_;
    // Ordered small, medium, large.
    private JTextField[] xyStepTexts_ = new JTextField[] { new JTextField(), new JTextField(), new JTextField() };
    // Ordered small, medium.
    private JTextField[] zStepTexts_ = new JTextField[] { new JTextField(), new JTextField() };
    private JTextField[] rStepTexts_ = new JTextField[] { new JTextField(), new JTextField() };

    /**
     * Creates new form StageControlFrame
     *
     * @param gui the MM api
     */
    public StageControl4DFrame(Studio gui) {
        studio_ = gui;
        core_ = studio_.getCMMCore();
        stageMotionExecutor_ = Executors.newFixedThreadPool(3);

        rotationManager = new OpticalRotation(studio_);

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
        double[] xyStepSizes = new double[] { 1.0, 10.0, 100.0 };
        double pixelSize = core_.getPixelSizeUm();
        long nrPixelsX = core_.getImageWidth();
        if (pixelSize != 0) {
            xyStepSizes[0] = pixelSize;
            xyStepSizes[1] = pixelSize * nrPixelsX * 0.1;
            xyStepSizes[2] = pixelSize * nrPixelsX;
        }
        // Read XY stepsizes from profile
        for (int i = 0; i < 3; ++i) {
            xyStepSizes[i] = studio_.profile().getSettings(StageControl4DFrame.class).getDouble(XY_MOVEMENTS[i],
                    xyStepSizes[i]);
            xyStepTexts_[i].setText(NumberUtils.doubleToDisplayString(xyStepSizes[i]));
        }

        for (int i = 0; i < 3; ++i) {
            centers[i] = studio_.profile().getSettings(StageControl4DFrame.class).getDouble(CENTERS[i], centers[i]);
        }

        StrVector zDrives = core_.getLoadedDevicesOfType(DeviceType.StageDevice);
        StrVector xyDrives = core_.getLoadedDevicesOfType(DeviceType.XYStageDevice);

        boolean error = xyDrives.isEmpty() || zDrives.isEmpty() || zDrives.size() < 2;
        xyPanel_.setVisible(!error);
        zPanel_.setVisible(!error);
        rPanel_.setVisible(!error);
        controlPanel_.setVisible(!error);
        errorPanel_.setVisible(error);

        boolean zDriveFound = false;
        boolean rDriveFound = false;
        if (!error) {
            zDriveSelect_.setVisible(true);
            rDriveSelect_.setVisible(true);

            if (zDriveSelect_.getItemCount() != 0) {
                zDriveSelect_.removeAllItems();
            }
            if (rDriveSelect_.getItemCount() != 0) {
                rDriveSelect_.removeAllItems();
            }

            ActionListener[] zDriveActionListeners = zDriveSelect_.getActionListeners();
            for (ActionListener l : zDriveActionListeners) {
                zDriveSelect_.removeActionListener(l);
            }
            ActionListener[] rDriveActionListeners = rDriveSelect_.getActionListeners();
            for (ActionListener l : rDriveActionListeners) {
                rDriveSelect_.removeActionListener(l);
            }

            for (int i = 0; i < zDrives.size(); i++) {
                String drive = zDrives.get(i);
                zDriveSelect_.addItem(drive);
                rDriveSelect_.addItem(drive);
                if (rotationManager.getZStage().equals(zDrives.get(i))) {
                    zDriveFound = true;
                }
                if (rotationManager.getRStage().equals(zDrives.get(i))) {
                    rDriveFound = true;
                }
            }
            if (!zDriveFound) {
                rotationManager.setZStage(zDrives.get(0));
            } else {
                zDriveSelect_.setSelectedItem(rotationManager.getZStage());
            }
            if (!rDriveFound) {
                rotationManager.setRStage(zDrives.get(1));
            } else {
                rDriveSelect_.setSelectedItem(rotationManager.getRStage());
            }
            for (ActionListener l : zDriveActionListeners) {
                zDriveSelect_.addActionListener(l);
            }
            for (ActionListener l : rDriveActionListeners) {
                rDriveSelect_.addActionListener(l);
            }
            updateZMovements();
            updateRMovements();

        }

        initialized_ = true;

        if (xyDrives.size() != 0) {
            try {
                getXYPosLabelFromCore();
            } catch (Exception e) {
                studio_.logs().logError(e, "Unable to get XY stage position");
            }
        }
        // guarantee that the z-position shown is correct:
        if (zDriveFound) {
            updateZDriveInfo();
        }
        if (rDriveFound) {
            updateRDriveInfo();
        }

        pack();
    }

    private void updateZMovements() {
        double smallMovement = studio_.profile().getSettings(StageControl4DFrame.class)
                .getDouble(SMALLMOVEMENTZ + rotationManager.getZStage(), 1.0);
        zStepTexts_[0].setText(NumberUtils.doubleToDisplayString(smallMovement));
        double mediumMovement = studio_.profile().getSettings(StageControl4DFrame.class)
                .getDouble(MEDIUMMOVEMENTZ + rotationManager.getZStage(), 10.0);
        zStepTexts_[1].setText(NumberUtils.doubleToDisplayString(mediumMovement));
    }

    private void updateRMovements() {
        double smallMovement = studio_.profile().getSettings(StageControl4DFrame.class)
                .getDouble(SMALLMOVEMENTR + rotationManager.getRStage(), 1.0);
        rStepTexts_[0].setText(NumberUtils.doubleToDisplayString(smallMovement));
        double mediumMovement = studio_.profile().getSettings(StageControl4DFrame.class)
                .getDouble(MEDIUMMOVEMENTR + rotationManager.getRStage(), 10.0);
        rStepTexts_[1].setText(NumberUtils.doubleToDisplayString(mediumMovement));
    }

    private void initComponents() {
        setTitle("4D stage controller");
        setLocationByPlatform(true);
        setResizable(false);
        setLayout(new MigLayout("fill, insets 5, gap 2"));

        xyPanel_ = createXYPanel();
        add(xyPanel_, "hidemode 2");

        // Vertically align Z panel with XY panel. createZPanel() also makes
        // several assumptions about the layout of the XY panel so that its
        // components are nicely vertically aligned.
        zPanel_ = createZPanel();
        add(zPanel_, "aligny top, gapleft 20, hidemode 2");

        rPanel_ = createRPanel();
        add(rPanel_, "aligny top, gapleft 20, hidemode 2");

        controlPanel_ = createControlPanel();
        add(controlPanel_, "aligny top, gapleft 20, hidemode 2");

        errorPanel_ = createErrorPanel();
        add(errorPanel_, "grow, hidemode 2");

    }

    private JPanel createXYPanel() {
        final JFrame theWindow = this;
        JPanel result = new JPanel(new MigLayout("insets 0, gap 0"));
        result.add(new JLabel("XY Stage", JLabel.CENTER), "span, alignx center, gapbottom 5, wrap");

        // Create a layout of buttons like this:
        // ^
        // ^
        // ^
        // <<< >>>
        // v
        // v
        // v
        // Doing this in a reasonably non-redundant, compact way is somewhat
        // tricky as each button is subtly different: they have different icons
        // and move the stage in different directions by different amounts when
        // pressed. We'll define them by index number 0-12: first all the "up"
        // buttons, then all "left" buttons, then all "right" buttons, then all
        // "down" buttons, so i / 4 indicates direction and i % 3 indicates step
        // size (with a minor wrinkle noted later).
        // Utility arrays for icon filenames.
        // Presumably for "single", "double", and "triple".
        String[] stepSizes = new String[] { "s", "d", "t" };
        // Up, left, right, down.
        String[] directions = new String[] { "u", "l", "r", "d" };
        for (int i = 0; i < 12; ++i) {
            // "Right" and "Down" buttons are ordered differently; in any case,
            // the largest-step button is furthest from the center.
            final int stepIndex = (i <= 5) ? (2 - (i % 3)) : (i % 3);
            String path = "/org/micromanager/icons/stagecontrol/arrowhead-" + stepSizes[stepIndex] + directions[i / 3];
            final JButton button = new JButton(IconLoader.getIcon(path + ".png"));
            // This copy can be referred to in the action listener.
            final int index = i;
            button.setBorder(null);
            button.setBorderPainted(false);
            button.setContentAreaFilled(false);
            button.setPressedIcon(IconLoader.getIcon(path + "p.png"));
            button.addActionListener(new ActionListener() {
                @Override
                public void actionPerformed(ActionEvent e) {
                    int dx = 0;
                    int dy = 0;
                    switch (index / 3) {
                    case 0:
                        dy = -1;
                        break;
                    case 1:
                        dx = -1;
                        break;
                    case 2:
                        dx = 1;
                        break;
                    case 3:
                        dy = 1;
                        break;
                    }
                    try {
                        double increment = NumberUtils.displayStringToDouble(xyStepTexts_[stepIndex].getText());
                        setRelativeXYStagePosition(dx * increment, dy * increment);
                    } catch (ParseException ex) {
                        JOptionPane.showMessageDialog(theWindow, "XY Step size is not a number");
                    }

                }
            });
            // Add the button to the panel.
            String constraint = "";
            if (i < 3 || i > 8) {
                // Up or down button.
                constraint = "span, alignx center, wrap";
            } else if (i == 3) {
                // First horizontal button
                constraint = "split, span";
            } else if (i == 6) {
                // Fourth horizontal button (start of the "right" buttons); add
                // a gap to the left.
                constraint = "gapleft 30";
            } else if (i == 8) {
                // Last horizontal button.
                constraint = "wrap";
            }
            result.add(button, constraint);
        }
        // Add the XY position label in the upper-left.
        xyPositionLabel_ = new JLabel("", JLabel.LEFT);
        result.add(xyPositionLabel_, "pos 5 20, width 120!, alignx left");

        // Gap between the chevrons and the step size controls.
        result.add(new JLabel(), "height 20!, wrap");

        // Now the controls for setting the step size.
        String[] labels = new String[] { "1 pixel", "0.1 field", "1 field" };
        for (int i = 0; i < 3; ++i) {
            JLabel indicator = new JLabel(
                    IconLoader.getIcon("/org/micromanager/icons/stagecontrol/arrowhead-" + stepSizes[i] + "r.png"));
            // HACK: make it smaller so the gap between rows is smaller.
            result.add(indicator, "height 20!, split, span");
            // This copy can be referred to in the action listener.
            final int index = i;

            // See above HACK note.
            result.add(xyStepTexts_[i], "height 20!, width 80");

            result.add(new JLabel("\u00b5m"));

            JButton presetButton = new JButton(labels[i]);
            presetButton.setFont(new Font("Arial", Font.PLAIN, 10));
            presetButton.addActionListener(new ActionListener() {
                @Override
                public void actionPerformed(ActionEvent e) {
                    double pixelSize = core_.getPixelSizeUm();
                    double viewSize = core_.getImageWidth() * pixelSize;
                    double[] sizes = new double[] { pixelSize, viewSize / 10, viewSize };
                    double stepSize = sizes[index];
                    xyStepTexts_[index].setText(NumberUtils.doubleToDisplayString(stepSize));
                }
            });
            result.add(presetButton, "width 80!, height 20!, wrap");
        } // End creating set-step-size text fields/buttons.

        return result;
    }

    /**
     * NOTE: this method makes assumptions about the layout of the XY panel. In
     * particular, it is assumed that each chevron button is 30px tall, that the
     * step size controls are 20px tall, and that there is a 20px gap between the
     * chevrons and the step size controls.
     */
    private JPanel createZPanel() {
        final JFrame theWindow = this;
        JPanel result = new JPanel(new MigLayout("insets 0, gap 0, flowy"));
        result.add(new JLabel("Z Stage", JLabel.CENTER), "growx, alignx center, gapbottom 5");
        zDriveSelect_ = new JComboBox();
        zDriveSelect_.addActionListener(new ActionListener() {
            @Override
            public void actionPerformed(ActionEvent e) {
                updateZDriveInfo();
            }
        });
        // HACK: this defined height for the combobox matches the height of one
        // of the chevron buttons, and helps to align components between the XY
        // and Z panels.
        result.add(zDriveSelect_, "height 30!, hidemode 0, growx");

        // Create buttons for stepping up/down.
        // Icon name prefix: double, single, single, double
        String[] prefixes = new String[] { "d", "s", "s", "d" };
        // Icon name component: up, up, down, down
        String[] directions = new String[] { "u", "u", "d", "d" };
        for (int i = 0; i < 4; ++i) {
            String path = "/org/micromanager/icons/stagecontrol/arrowhead-" + prefixes[i] + directions[i];
            JButton button = new JButton(IconLoader.getIcon(path + ".png"));
            button.setBorder(null);
            button.setBorderPainted(false);
            button.setContentAreaFilled(false);
            button.setPressedIcon(IconLoader.getIcon(path + "p.png"));
            // This copy can be referred to in the action listener.
            final int index = i;
            button.addActionListener(new ActionListener() {
                @Override
                public void actionPerformed(ActionEvent e) {
                    int dz = (index < 2) ? 1 : -1;
                    double stepSize;
                    JTextField text = (index == 0 || index == 3) ? zStepTexts_[1] : zStepTexts_[0];
                    try {
                        stepSize = NumberUtils.displayStringToDouble(text.getText());
                    } catch (ParseException ex) {
                        JOptionPane.showMessageDialog(theWindow, "Z-step value is not a number");
                        return;
                    }
                    setRelativeZStagePosition(dz * stepSize);
                }
            });
            result.add(button, "alignx center, growx");
            if (i == 1) {
                // Stick the Z position text in the middle.
                // HACK: As above HACK, this height matches the height of the
                // chevron buttons in the XY panel.
                zPositionLabel_ = new JLabel("", JLabel.CENTER);
                result.add(zPositionLabel_, "height 30!, width 100:, alignx center, growx");
            }
        }

        // Spacer to vertically align stepsize controls with the XY panel.
        // Encompasses one chevron (height 30) and the gap the XY panel has
        // (height 20).
        result.add(new JLabel(), "height 50!");

        // Create the controls for setting the step size.
        // These heights again must match those of the corresponding stepsize
        // controls in the XY panel.
        double size = studio_.profile().getSettings(StageControl4DFrame.class)
                .getDouble(SMALLMOVEMENTZ + rotationManager.getZStage(), 1.0);

        zStepTexts_[0].setText(NumberUtils.doubleToDisplayString(size));
        result.add(new JLabel(IconLoader.getIcon("/org/micromanager/icons/stagecontrol/arrowhead-sr.png")),
                "height 20!, span, split 3, flowx");
        result.add(zStepTexts_[0], "height 20!, width 50");
        result.add(new JLabel("\u00b5m"), "height 20!");

        size = studio_.profile().getSettings(StageControl4DFrame.class)
                .getDouble(MEDIUMMOVEMENTZ + rotationManager.getZStage(), 10.0);
        zStepTexts_[1].setText(NumberUtils.doubleToDisplayString(size));
        result.add(new JLabel(IconLoader.getIcon("/org/micromanager/icons/stagecontrol/arrowhead-dr.png")),
                "span, split 3, flowx");
        result.add(zStepTexts_[1], "width 50");
        result.add(new JLabel("\u00b5m"));

        return result;
    }

    private JPanel createRPanel() {
        final JFrame theWindow = this;
        JPanel result = new JPanel(new MigLayout("insets 0, gap 0, flowy"));
        result.add(new JLabel("R Stage", JLabel.CENTER), "growx, alignx center, gapbottom 5");
        rDriveSelect_ = new JComboBox();
        rDriveSelect_.addActionListener(new ActionListener() {
            @Override
            public void actionPerformed(ActionEvent e) {
                updateRDriveInfo();
            }
        });
        // HACK: this defined height for the combobox matches the height of one
        // of the chevron buttons, and helps to align components between the XY
        // and Z panels.
        result.add(rDriveSelect_, "height 30!, hidemode 0, growx");

        // Create buttons for stepping up/down.
        // Icon name prefix: double, single, single, double
        String[] prefixes = new String[] { "d", "s", "s", "d" };
        // Icon name component: up, up, down, down
        String[] directions = new String[] { "u", "u", "d", "d" };
        for (int i = 0; i < 4; ++i) {
            String path = "/org/micromanager/icons/stagecontrol/arrowhead-" + prefixes[i] + directions[i];
            JButton button = new JButton(IconLoader.getIcon(path + ".png"));
            button.setBorder(null);
            button.setBorderPainted(false);
            button.setContentAreaFilled(false);
            button.setPressedIcon(IconLoader.getIcon(path + "p.png"));
            // This copy can be referred to in the action listener.
            final int index = i;
            button.addActionListener(new ActionListener() {
                @Override
                public void actionPerformed(ActionEvent e) {
                    int dr = (index < 2) ? 1 : -1;
                    double stepSize;
                    JTextField text = (index == 0 || index == 3) ? rStepTexts_[1] : rStepTexts_[0];
                    try {
                        stepSize = NumberUtils.displayStringToDouble(text.getText());
                    } catch (ParseException ex) {
                        JOptionPane.showMessageDialog(theWindow, "R-step value is not a number");
                        return;
                    }
                    setRelativeRStagePosition(dr * stepSize);
                }
            });
            result.add(button, "alignx center, growx");
            if (i == 1) {
                // Stick the Z position text in the middle.
                // HACK: As above HACK, this height matches the height of the
                // chevron buttons in the XY panel.
                rPositionLabel_ = new JLabel("", JLabel.CENTER);
                result.add(rPositionLabel_, "height 30!, width 100:, alignx center, growx");
            }
        }

        result.add(new JLabel(), "height 50!");

        // Create the controls for setting the step size.
        // These heights again must match those of the corresponding stepsize
        // controls in the XY panel.
        double size = studio_.profile().getSettings(StageControl4DFrame.class)
                .getDouble(SMALLMOVEMENTR + rotationManager.getRStage(), 1.0);

        rStepTexts_[0].setText(NumberUtils.doubleToDisplayString(size));
        result.add(new JLabel(IconLoader.getIcon("/org/micromanager/icons/stagecontrol/arrowhead-sr.png")),
                "height 20!, span, split 3, flowx");
        result.add(rStepTexts_[0], "height 20!, width 50");
        result.add(new JLabel("\u00b0"), "height 20!");

        size = studio_.profile().getSettings(StageControl4DFrame.class)
                .getDouble(MEDIUMMOVEMENTR + rotationManager.getRStage(), 10.0);
        rStepTexts_[1].setText(NumberUtils.doubleToDisplayString(size));
        result.add(new JLabel(IconLoader.getIcon("/org/micromanager/icons/stagecontrol/arrowhead-dr.png")),
                "span, split 3, flowx");
        result.add(rStepTexts_[1], "width 50");
        result.add(new JLabel("\u00b0"));

        return result;
    }

    private JPanel createControlPanel() {
        JPanel result = new JPanel(new MigLayout("insets 0, gap 0, flowy"));
        result.add(new JLabel("Translation", JLabel.CENTER), "growx, alignx center");

        JButton presetButton = new JButton("Home XY");
        presetButton.setFont(new Font("Arial", Font.PLAIN, 10));
        presetButton.addActionListener(new ActionListener() {
            @Override
            public void actionPerformed(ActionEvent e) {
                homeXYStage();
            }
        });
        result.add(presetButton, "width 100!, height 20!, gaptop 5");

        presetButton = new JButton("Home Z");
        presetButton.setFont(new Font("Arial", Font.PLAIN, 10));
        presetButton.addActionListener(new ActionListener() {
            @Override
            public void actionPerformed(ActionEvent e) {
                homeZStage();
            }
        });
        result.add(presetButton, "width 100!, height 20!, gaptop 5");

        presetButton = new JButton("Center");
        presetButton.setFont(new Font("Arial", Font.PLAIN, 10));
        presetButton.addActionListener(new ActionListener() {
            @Override
            public void actionPerformed(ActionEvent e) {
                centerStage();
            }
        });
        result.add(presetButton, "width 100!, height 20!, gaptop 10");

        presetButton = new JButton("Save center");
        presetButton.setFont(new Font("Arial", Font.PLAIN, 10));
        presetButton.addActionListener(new ActionListener() {
            @Override
            public void actionPerformed(ActionEvent e) {
                int result = JOptionPane.showConfirmDialog(null, "Save current as center?", "Save center",
                        JOptionPane.YES_NO_OPTION);
                if (result == JOptionPane.YES_OPTION) {
                    registerCenter();
                }
            }
        });
        result.add(presetButton, "width 100!, height 20!, gaptop 10");

        result.add(new JLabel("Rotation", JLabel.CENTER), "growx, alignx center, gaptop 25");

        presetButton = new JButton("Zero R");
        presetButton.setFont(new Font("Arial", Font.PLAIN, 10));
        presetButton.addActionListener(new ActionListener() {
            @Override
            public void actionPerformed(ActionEvent e) {
                zeroRStage();
            }
        });
        result.add(presetButton, "width 100!, height 20!, gaptop 5");

        presetButton = new JButton("Calibrate");
        presetButton.setFont(new Font("Arial", Font.PLAIN, 10));
        presetButton.addActionListener(new ActionListener() {
            @Override
            public void actionPerformed(ActionEvent e) {
                openCalibration();
            }
        });
        result.add(presetButton, "width 100!, height 20!, gaptop 5");

        presetButton = new JButton("RECOVERY");
        presetButton.setFont(new Font("Arial", Font.BOLD, 10));
        presetButton.addActionListener(new ActionListener() {
            @Override
            public void actionPerformed(ActionEvent e) {
                recoverStage();
            }
        });
        result.add(presetButton, "width 100!, height 30!, gaptop 100");

        return result;
    }

    private void openCalibration() {
        if (calibrationFrame_ == null) {
            calibrationFrame_ = new CalibrationFrame(this, studio_);
        }
        calibrationFrame_.open();
    }

    private JPanel createErrorPanel() {
        // Provide a friendly message when there are no drives in the device list
        JLabel noDriveLabel = new javax.swing.JLabel("No XY, Z drive and twister found.  Nothing to control.");
        noDriveLabel.setOpaque(true);

        JPanel panel = new JPanel(new MigLayout("fill"));
        panel.add(noDriveLabel, "align center, grow");

        return panel;
    }

    private void storeZValuesInProfile() {
        try {
            double stepSize = NumberUtils.displayStringToDouble(zStepTexts_[0].getText());
            studio_.profile().getSettings(StageControl4DFrame.class)
                    .putDouble(SMALLMOVEMENTZ + rotationManager.getZStage(), stepSize);
        } catch (ParseException pe) {// ignore, it would be annoying to ask for user input}
        }
        try {
            double stepSize = NumberUtils.displayStringToDouble(zStepTexts_[1].getText());
            studio_.profile().getSettings(StageControl4DFrame.class)
                    .putDouble(MEDIUMMOVEMENTZ + rotationManager.getZStage(), stepSize);
        } catch (ParseException pe) {// ignore, it would be annoying to ask for user input}
        }
    }

    private void storeRValuesInProfile() {
        try {
            double stepSize = NumberUtils.displayStringToDouble(rStepTexts_[0].getText());
            studio_.profile().getSettings(StageControl4DFrame.class)
                    .putDouble(SMALLMOVEMENTR + rotationManager.getRStage(), stepSize);
        } catch (ParseException pe) {// ignore, it would be annoying to ask for user input}
        }
        try {
            double stepSize = NumberUtils.displayStringToDouble(rStepTexts_[1].getText());
            studio_.profile().getSettings(StageControl4DFrame.class)
                    .putDouble(MEDIUMMOVEMENTR + rotationManager.getRStage(), stepSize);
        } catch (ParseException pe) {// ignore, it would be annoying to ask for user input}
        }
    }

    private void updateZDriveInfo() {
        // First store current Z step sizes:
        storeZValuesInProfile();
        // then update the current Z Drive
        String curDrive = (String) zDriveSelect_.getSelectedItem();
        if (curDrive != null && initialized_) {
            rotationManager.setZStage(curDrive);
            // Remember step sizes for this new drive.
            updateZMovements();
            try {
                getZPosLabelFromCore();
            } catch (Exception ex) {
                studio_.logs().logError(ex,
                        "Failed to pull position from core for Z drive " + rotationManager.getZStage());
            }
        }
    }

    private void updateRDriveInfo() {
        // First store current Z step sizes:
        storeRValuesInProfile();
        // then update the current Z Drive
        String curDrive = (String) rDriveSelect_.getSelectedItem();
        if (curDrive != null && initialized_) {
            rotationManager.setRStage(curDrive);
            // Remember step sizes for this new drive.
            updateRMovements();
            try {
                getRPosLabelFromCore();
            } catch (Exception ex) {
                studio_.logs().logError(ex,
                        "Failed to pull position from core for R drive " + rotationManager.getRStage());
            }
        }
    }

    private void setRelativeXYStagePosition(double x, double y) {
        try {
            if (!core_.deviceBusy(core_.getXYStageDevice())) {
                StageThread st = new StageThread(core_.getXYStageDevice(), x, y);
                stageMotionExecutor_.execute(st);
            }
        } catch (Exception e) {
            studio_.logs().logError(e);
        }
    }

    private void setRelativeZStagePosition(double z) {
        try {
            String currentZDrive_ = rotationManager.getZStage();
            if (!core_.deviceBusy(currentZDrive_)) {
                StageThread st = new StageThread(currentZDrive_, z);
                stageMotionExecutor_.execute(st);
            }
        } catch (Exception ex) {
            studio_.logs().showError(ex);
        }
    }

    private void setRelativeRStagePosition(double r) {
        try {
            String currentRDrive_ = rotationManager.getRStage();
            if (!core_.deviceBusy(currentRDrive_)) {
                RotationThread st = new RotationThread(r);
                stageMotionExecutor_.execute(st);
            }
        } catch (Exception ex) {
            studio_.logs().showError(ex);
        }
    }

    private void recoverStage() {
        try {
            core_.initializeDevice(core_.getXYStageDevice());
            core_.initializeDevice(rotationManager.getRStage());
            core_.initializeDevice(rotationManager.getZStage());
            studio_.logs().logMessage("Stage drivers reloaded");
            JOptionPane.showMessageDialog(this, "Device recovery successful");
        } catch (Exception ex) {
            studio_.logs().showError(ex);
        }
    }

    private void homeXYStage() {
        try {
            if (!core_.deviceBusy(core_.getXYStageDevice())) {
                core_.home(core_.getXYStageDevice());
                core_.waitForDevice(core_.getXYStageDevice());

                getXYPosLabelFromCore();

            }
        } catch (Exception ex) {
            studio_.logs().showError(ex);
        }
    }

    private void homeZStage() {
        try {
            String currentZDrive_ = rotationManager.getZStage();
            if (!core_.deviceBusy(currentZDrive_)) {
                core_.home(currentZDrive_);
                core_.waitForDevice(currentZDrive_);

                getZPosLabelFromCore();

            }
        } catch (Exception ex) {
            studio_.logs().showError(ex);
        }
    }

    private void centerStage() {
        try {
            core_.waitForDevice(core_.getXYStageDevice());
            core_.setXYPosition(centers[0], centers[1]);
            core_.waitForDevice(core_.getXYStageDevice());

            core_.waitForDevice(rotationManager.getZStage());
            core_.setPosition(rotationManager.getZStage(), centers[2]);
            core_.waitForDevice(rotationManager.getZStage());

            getXYPosLabelFromCore();
            getZPosLabelFromCore();

        } catch (Exception ex) {
            studio_.logs().logError(ex);
        }
    }

    private void registerCenter() {
        try {
            Point2D.Double pos = core_.getXYStagePosition(core_.getXYStageDevice());
            double zPos = core_.getPosition(rotationManager.getZStage());

            centers[0] = pos.x;
            centers[1] = pos.y;
            centers[2] = zPos;

            for (int i = 0; i < 3; ++i) {
                studio_.profile().getSettings(StageControl4DFrame.class).putDouble(CENTERS[i], centers[i]);
            }
        } catch (Exception ex) {
            studio_.logs().showError(ex);
        }
    }

    private void zeroRStage() {
        try {
            String currentRDrive_ = rotationManager.getRStage();
            if (!core_.deviceBusy(currentRDrive_)) {
                core_.setOrigin(currentRDrive_);
                core_.waitForDevice(currentRDrive_);

                core_.sleep(250);
                getRPosLabelFromCore();
            }
        } catch (Exception ex) {
            studio_.logs().showError(ex);
        }
    }

    private void getXYPosLabelFromCore() throws Exception {
        Point2D.Double pos = core_.getXYStagePosition(core_.getXYStageDevice());
        setXYPosLabel(pos.x, pos.y);
    }

    private void setXYPosLabel(double x, double y) {
        xyPositionLabel_.setText(String.format("<html>X: %s \u00b5m<br>Y: %s \u00b5m</html>",
                TextUtils.removeNegativeZero(NumberUtils.doubleToDisplayString(x)),
                TextUtils.removeNegativeZero(NumberUtils.doubleToDisplayString(y))));
    }

    private void getZPosLabelFromCore() throws Exception {
        double zPos = core_.getPosition(rotationManager.getZStage());
        setZPosLabel(zPos);
    }

    private void getRPosLabelFromCore() throws Exception {
        double rPos = core_.getPosition(rotationManager.getRStage());
        setRPosLabel(rPos);
    }

    private void setZPosLabel(double z) {
        zPositionLabel_.setText(TextUtils.removeNegativeZero(NumberUtils.doubleToDisplayString(z)) + " \u00B5m");
    }

    private void setRPosLabel(double r) {
        rPositionLabel_.setText(TextUtils.removeNegativeZero(NumberUtils.doubleToDisplayString(r)) + " \u00B0");
    }

    @Subscribe
    public void onSystemConfigurationLoaded(SystemConfigurationLoadedEvent event) {
        initialize();
    }

    @Subscribe
    public void onStagePositionChanged(StagePositionChangedEvent event) {
        if (event.getDeviceName().contentEquals(rotationManager.getZStage())) {
            setZPosLabel(event.getPos());
        } else if (event.getDeviceName().contentEquals(rotationManager.getRStage())) {
            setRPosLabel(event.getPos());
        }
    }

    @Subscribe
    public void onXYStagePositionChanged(XYStagePositionChangedEvent event) {
        if (event.getDeviceName().contentEquals(core_.getXYStageDevice())) {
            setXYPosLabel(event.getXPos(), event.getYPos());
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
        for (int i = 0; i < 3; i++) {
            try {
                studio_.profile().getSettings(StageControl4DFrame.class).putDouble(XY_MOVEMENTS[i],
                        NumberUtils.displayStringToDouble(xyStepTexts_[i].getText()));
            } catch (ParseException pex) {
                // since we are closing, no need to warn the user
            }
        }
        storeZValuesInProfile();
        storeRValuesInProfile();

        super.dispose();
    }

    private class StageThread implements Runnable {

        final String device_;
        final boolean isXYStage_;
        final double x_;
        final double y_;
        final double z_;

        public StageThread(String device, double z) {
            device_ = device;
            z_ = z;
            x_ = y_ = 0;
            isXYStage_ = false;
        }

        public StageThread(String device, double x, double y) {
            device_ = device;
            x_ = x;
            y_ = y;
            z_ = 0;
            isXYStage_ = true;
        }

        @Override
        public void run() {
            try {
                core_.waitForDevice(device_);
                if (isXYStage_) {
                    core_.setRelativeXYPosition(device_, x_, y_);
                } else {
                    core_.setRelativePosition(device_, z_);
                }
                core_.waitForDevice(device_);
                if (isXYStage_) {
                    getXYPosLabelFromCore();
                } else {
                    getZPosLabelFromCore();
                }
            } catch (Exception ex) {
                studio_.logs().logError(ex);
            }
        }
    }

    private class RotationThread implements Runnable {

        final double angle_;

        public RotationThread(double angle) {
            angle_ = angle;
        }

        @Override
        public void run() {
            try {
                rotationManager.rotate(angle_);

                getXYPosLabelFromCore();
                getZPosLabelFromCore();
                getRPosLabelFromCore();

            } catch (Exception ex) {
                studio_.logs().logError(ex);
            }
        }
    }
}