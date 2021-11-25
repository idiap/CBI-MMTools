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
import java.awt.Component;
import java.awt.Font;
import java.util.ArrayList;
import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;
import java.awt.geom.Point2D;
import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.io.PrintWriter;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.Arrays;
import javax.swing.ButtonGroup;
import javax.swing.JButton;
import javax.swing.JCheckBox;
import javax.swing.JFileChooser;
import javax.swing.JLabel;
import javax.swing.JOptionPane;
import javax.swing.JRadioButton;
import javax.swing.JTextField;
import javax.swing.JFrame;
import mmcorej.CMMCore;
import net.miginfocom.swing.MigLayout;
import org.apache.commons.math3.analysis.MultivariateFunction;
import org.apache.commons.math3.linear.EigenDecomposition;
import org.apache.commons.math3.linear.RealMatrix;
import org.apache.commons.math3.linear.RealVector;
import org.apache.commons.math3.optim.InitialGuess;
import org.apache.commons.math3.optim.MaxEval;
import org.apache.commons.math3.optim.MaxIter;
import org.apache.commons.math3.optim.PointValuePair;
import org.apache.commons.math3.optim.nonlinear.scalar.GoalType;
import org.apache.commons.math3.optim.nonlinear.scalar.ObjectiveFunction;
import org.apache.commons.math3.optim.nonlinear.scalar.noderiv.NelderMeadSimplex;
import org.apache.commons.math3.optim.nonlinear.scalar.noderiv.SimplexOptimizer;
import org.apache.commons.math3.stat.descriptive.MultivariateSummaryStatistics;
import org.apache.commons.math3.util.Precision;
import org.micromanager.Studio;

public class CalibrationFrame extends JFrame {

    private JLabel currentCalibrationLabel;
    private JLabel currentTiltLabel;
    private JTextField manualCZ;
    private JTextField manualCX;
    private JLabel pointsLabel;
    private JButton calibrateButton;
    private JButton tiltButton;
    private JRadioButton rotationDirButton1;
    private JRadioButton rotationDirButton2;
    private JCheckBox debugTick;
    private ArrayList<PointValuePair> points_;
    private final Component parent_;
    private final Studio studio_;
    private final CMMCore core_;
    private final OpticalRotation rotationManager;

    private static final double OPTIM_REL_THRESH = 1e-12;
    private static final double OPTIM_REL_THRESH_PRECISE = 1e-15;
    private static final double OPTIM_ABS_THRESH = 0;
    private static final int OPTIM_MAX_ITER = 500000;
    private static final double Z_ERROR_WEIGHT = 0.7;

    public CalibrationFrame(Component parent, Studio studio) {
        parent_ = parent;
        studio_ = studio;
        core_ = studio.getCMMCore();
        rotationManager = new OpticalRotation(studio);
        initComponents();
    }

    public void open() {
        points_ = new ArrayList<>();
        updateStoredPoints();
        updateCalibrationLabel();
        this.setVisible(true);
    }

    private void clearPoints() {
        points_.clear();
        updateStoredPoints();
    }

    private void updateStoredPoints() {
        int storedPoints_ = points_.size();
        pointsLabel.setText(Integer.toString(storedPoints_));
        if (storedPoints_ > 1) {
            calibrateButton.setEnabled(true);
            tiltButton.setEnabled(true);
        } else {
            calibrateButton.setEnabled(false);
            tiltButton.setEnabled(false);
        }
    }

    private void reset() {
        clearPoints();
        rotationManager.registerCalibration(0.0, 0.0, 1, false);
        rotationManager.registerTilt(0.0, 0.0);
        // this.dispose();
    }

    private void compute_tilt(boolean success_frame) {
        MultivariateSummaryStatistics stats = new MultivariateSummaryStatistics(3, false);
        for (PointValuePair point : points_) {
            stats.addValue(point.getPoint());
        }

        RealMatrix covar = stats.getCovariance();
        EigenDecomposition eigen = new EigenDecomposition(covar);

        RealVector axis = eigen.getEigenvector(2);

        double dir_z = axis.getEntry(0);
        double dir_x = axis.getEntry(1);
        double dir_y = axis.getEntry(2);

        double theta_xy = Math.toDegrees(Math.acos(dir_y / Math.sqrt(Math.pow(dir_y, 2) + Math.pow(dir_x, 2))));
        double theta_zy = Math.toDegrees(Math.acos(dir_y / Math.sqrt(Math.pow(dir_y, 2) + Math.pow(dir_z, 2))));

        rotationManager.registerTilt(theta_zy, theta_xy);
        updateCalibrationLabel();

        if (success_frame) {
            JOptionPane.showMessageDialog(null, "Calibration successful! " + tiltString());
        }
    }

    private void calibrate() {
        double[] initPoint;
        if (rotationManager.getCalibrated()) {
            initPoint = rotationManager.getCentre();
        } else {
            MultivariateSummaryStatistics stats = new MultivariateSummaryStatistics(3, false);
            for (PointValuePair point : points_) {
                stats.addValue(point.getPoint());
            }
            initPoint = stats.getMean();
        }

        double meanDist = 0.0;
        for (PointValuePair point : points_) {
            double[] stored = point.getPoint();
            meanDist += Math.sqrt(Math.pow(initPoint[0] - stored[0], 2) + Math.pow(initPoint[1] - stored[1], 2));
        }
        meanDist /= points_.size();

        int direction;
        if (rotationDirButton1.isSelected()) {
            direction = 1;
        } else {
            direction = -1;
        }

        SimplexOptimizer optim = new SimplexOptimizer(OPTIM_REL_THRESH, OPTIM_ABS_THRESH);
        PointValuePair result = optim.optimize(new NelderMeadSimplex(4), GoalType.MINIMIZE,
                new InitialGuess(new double[] { initPoint[0], initPoint[1], meanDist, 0.0 }),
                new ObjectiveFunction(new CircleFittingFunction(points_, direction, Z_ERROR_WEIGHT)),
                MaxEval.unlimited(), new MaxIter(OPTIM_MAX_ITER));

        optim = new SimplexOptimizer(OPTIM_REL_THRESH_PRECISE, OPTIM_ABS_THRESH);
        result = optim.optimize(new NelderMeadSimplex(4), GoalType.MINIMIZE,
                new InitialGuess(new double[] { result.getPoint()[0], result.getPoint()[1], result.getPoint()[2],
                        result.getPoint()[3] }),
                new ObjectiveFunction(new CircleFittingFunction(points_, direction, Z_ERROR_WEIGHT)),
                MaxEval.unlimited(), new MaxIter(OPTIM_MAX_ITER));

        double centreZ = Precision.round(result.getPoint()[0], 2);
        double centreX = Precision.round(result.getPoint()[1], 2);

        rotationManager.registerCalibration(centreZ, centreX, direction, true);

        JOptionPane.showMessageDialog(null, "Calibration successful! " + calibrationString() + " -- " + tiltString());
        // this.dispose();

        if (debugTick.isSelected()) {
            JFileChooser fc = new JFileChooser();
            fc.setFileSelectionMode(JFileChooser.DIRECTORIES_ONLY);

            if (fc.showOpenDialog(null) == JFileChooser.APPROVE_OPTION) {
                String spath = fc.getSelectedFile().getAbsolutePath();
                Path path = Paths.get(spath);
                try {
                    File file = new File(path.resolve("points.txt").toString());
                    file.createNewFile();
                    FileWriter writer = new FileWriter(file);
                    PrintWriter printWriter = new PrintWriter(writer);

                    for (PointValuePair point : points_) {
                        double[] stored = point.getPoint();
                        printWriter.printf("%f %f %f %f\n", Math.toDegrees(point.getValue() * direction), stored[0],
                                stored[1], stored[2]);
                    }
                    printWriter.close();

                    file = new File(path.resolve("calibration.txt").toString());
                    file.createNewFile();
                    writer = new FileWriter(file);
                    printWriter = new PrintWriter(writer);

                    printWriter.printf("%f %f", centreZ, centreX);
                    printWriter.close();

                    JOptionPane.showMessageDialog(null, "Debug info saved.");

                } catch (IOException e) {
                    core_.logMessage("An error occurred.");
                }

            }
        }
    }

    private void manualCalibrate() {
        try {
            double centreZ = Double.parseDouble(manualCZ.getText());
            double centreX = Double.parseDouble(manualCX.getText());

            int direction;
            if (rotationDirButton1.isSelected()) {
                direction = 1;
            } else {
                direction = -1;
            }
            rotationManager.registerCalibration(centreZ, centreX, direction, true);
            JOptionPane.showMessageDialog(null, "Calibration successful! " + calibrationString());
            // this.dispose();
        } catch (NumberFormatException e) {
            JOptionPane.showMessageDialog(null, "Wrong number formatting");
        }
    }

    private String calibrationString() {
        if (!rotationManager.getCalibrated()) {
            return "none";
        }

        double[] centre = rotationManager.getOpticalCentre();
        int direction = rotationManager.getRotationDirection();

        String centreS = "ZX: (" + Double.toString(centre[0]) + ";" + Double.toString(centre[1]) + ")";
        String dirS = "R: " + Integer.toString(direction);

        return centreS + " -- " + dirS;
    }

    private String tiltString() {
        double[] tilt = rotationManager.getTilt();
        return "X: " + Double.toString(tilt[1]) + "º -- Z: " + Double.toString(tilt[0]) + "º";
    }

    private void updateCalibrationLabel() {
        String label = "Current calibration -- " + calibrationString();
        String tilt = "Current tilt -- " + tiltString();
        currentCalibrationLabel.setText(label);
        currentTiltLabel.setText(tilt);
    }

    private void initComponents() {
        setTitle("Rotation axis calibration");
        setLocationRelativeTo(parent_);
        setResizable(false);

        this.setLayout(new MigLayout("insets 5, gap 10"));

        JLabel presetLabel = new JLabel("<html>Calibrate the rotation axis to match the optical axis.<br>"
                + "Centre a given precise point in your sample and press 'Add point'.<br>"
                + "Rotate the stage to an arbitrary, focus and centre the same point and repeat.<br>"
                + "Adding multiple points improves the calibration precision.<br>"
                + "Use the reticle overlay as a precise reference for centring.</html>");
        this.add(presetLabel, "span, wrap");

        currentCalibrationLabel = new JLabel();
        this.add(currentCalibrationLabel, "span, gaptop 10, wrap");

        currentTiltLabel = new JLabel();
        this.add(currentTiltLabel, "span, gaptop 5, wrap");
        updateCalibrationLabel();

        presetLabel = new JLabel("Points added:");
        this.add(presetLabel, "gaptop 15, split 2");

        pointsLabel = new JLabel("0");
        this.add(pointsLabel, "wrap");

        JButton presetButton = new JButton("Add point");
        presetButton.setFont(new Font("Arial", Font.PLAIN, 10));
        presetButton.addActionListener(new ActionListener() {
            @Override
            public void actionPerformed(ActionEvent e) {
                try {
                    double[] zx = rotationManager.getZXCoordinates();
                    Point2D.Double pos = core_.getXYStagePosition(core_.getXYStageDevice());
                    double[] zxy_coords = { zx[0], zx[1], pos.y };
                    double r = rotationManager.getRPosition(true);

                    points_.add(new PointValuePair(zxy_coords, r));
                    updateStoredPoints();
                } catch (Exception ex) {
                    studio_.logs().logError(ex);
                }
            }
        });
        this.add(presetButton, "width 100!, height 20!");

        presetButton = new JButton("Clear points");
        presetButton.setFont(new Font("Arial", Font.PLAIN, 10));
        presetButton.addActionListener(new ActionListener() {
            @Override
            public void actionPerformed(ActionEvent e) {
                clearPoints();
            }
        });
        this.add(presetButton, "width 100!, height 20!, wrap");

        rotationDirButton1 = new JRadioButton("1");
        presetLabel = new JLabel(IconLoader.getIcon("/ch/idiap/cbi/resource/rotation1.png"));
        rotationDirButton1.setSelected(rotationManager.getRotationDirection() == 1);

        this.add(rotationDirButton1, "height 40!, split 2, gaptop 10");
        this.add(presetLabel, "height 40!");

        rotationDirButton2 = new JRadioButton("-1");
        presetLabel = new JLabel(IconLoader.getIcon("/ch/idiap/cbi/resource/rotation2.png"));
        rotationDirButton2.setSelected(rotationManager.getRotationDirection() == -1);

        this.add(rotationDirButton2, "height 40!, split 2");
        this.add(presetLabel, "height 40!, wrap");

        ButtonGroup radioButtons = new ButtonGroup();
        radioButtons.add(rotationDirButton1);
        radioButtons.add(rotationDirButton2);

        calibrateButton = new JButton("Calibrate!");
        calibrateButton.setFont(new Font("Arial", Font.BOLD, 12));
        calibrateButton.addActionListener(new ActionListener() {
            @Override
            public void actionPerformed(ActionEvent e) {
                int result = JOptionPane.showConfirmDialog(null, "Start calibration?", "Calibrate!",
                        JOptionPane.YES_NO_OPTION);
                if (result == JOptionPane.YES_OPTION) {
                    compute_tilt(false);
                    calibrate();
                }
            }
        });
        calibrateButton.setEnabled(false);
        this.add(calibrateButton, "span 2, grow, height 40!, gaptop 20");

        debugTick = new JCheckBox("Debug info");
        debugTick.setSelected(false);
        this.add(debugTick, "wrap");

        tiltButton = new JButton("Just tilt.");
        tiltButton.setFont(new Font("Arial", Font.PLAIN, 12));
        tiltButton.addActionListener(new ActionListener() {
            @Override
            public void actionPerformed(ActionEvent e) {
                compute_tilt(true);
            }
        });
        tiltButton.setEnabled(false);
        this.add(tiltButton, "span 2, grow, height 40!, gaptop 10, wrap");

        presetButton = new JButton("Reset");
        presetButton.setFont(new Font("Arial", Font.ITALIC, 10));
        presetButton.addActionListener(new ActionListener() {
            @Override
            public void actionPerformed(ActionEvent e) {
                int result = JOptionPane.showConfirmDialog(null, "Do you want to reset the calibration?", "Reset",
                        JOptionPane.YES_NO_OPTION);
                if (result == JOptionPane.YES_OPTION) {
                    reset();
                }
            }
        });
        this.add(presetButton, "span 2, grow, height 20!, wrap");

        presetLabel = new JLabel("Z:");
        manualCZ = new JTextField("0.0");
        this.add(presetLabel, "gaptop 20, split 2");
        this.add(manualCZ, "growx");

        presetLabel = new JLabel("X:");
        manualCX = new JTextField("0.0");
        this.add(presetLabel, "split 2");
        this.add(manualCX, "growx");

        presetButton = new JButton("Manual set");
        presetButton.setFont(new Font("Arial", Font.ITALIC, 10));
        presetButton.addActionListener(new ActionListener() {
            @Override
            public void actionPerformed(ActionEvent e) {
                int result = JOptionPane.showConfirmDialog(null, "Manually set calibration?", "Calibrate!",
                        JOptionPane.YES_NO_OPTION);
                if (result == JOptionPane.YES_OPTION) {
                    manualCalibrate();
                }
            }
        });
        this.add(presetButton, "width 150!, wrap");

        pack();
    }

    public class CircleFittingFunction implements MultivariateFunction {

        private final ArrayList<PointValuePair> points_;
        private final int direction;
        private final double zErrorWeight;

        public CircleFittingFunction(ArrayList<PointValuePair> points, int rotationDir, double zErrorWeight) {
            points_ = points;
            direction = rotationDir;
            this.zErrorWeight = zErrorWeight;
        }

        private double[] computePoint(double[] centre, double rad, double theta0, double theta) {
            double angle = theta0 + theta;

            double z = centre[0] + rad * Math.cos(angle);
            double x = centre[1] + rad * Math.sin(angle);

            return new double[] { z, x };
        }

        @Override
        public double value(double[] doubles) {
            double[] centre = Arrays.copyOfRange(doubles, 0, 2);
            double radius = doubles[2];
            double theta0 = doubles[3];

            double MSE = 0.0;

            for (PointValuePair point : points_) {
                double[] coord = point.getPoint();
                double angle = point.getValue() * direction;
                double[] computed = computePoint(centre, radius, theta0, angle);

                MSE += zErrorWeight
                        * Math.sqrt(Math.pow(coord[0] - computed[0], 2) + Math.pow(coord[1] - computed[1], 2));
            }
            MSE /= points_.size();
            return MSE;
        }

    }

}
