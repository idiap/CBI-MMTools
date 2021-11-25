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

import mmcorej.CMMCore;
import mmcorej.DeviceType;
import mmcorej.StrVector;
import org.apache.commons.math3.linear.Array2DRowRealMatrix;
import org.micromanager.Studio;

public class OpticalRotation {

    private static final String CURRENTZDRIVE = "CURRENTZDRIVE";
    private static final String CURRENTRDRIVE = "CURRENTRDRIVE";
    private static final String CENTREX = "CENTREX";
    private static final String CENTREZ = "CENTREZ";
    private static final String TILTX = "TILTX";
    private static final String TILTZ = "TILTZ";
    private static final String ROTATIONDIR = "DIRECTION";
    private static final String CALIBRATED = "CALIBRATED";

    private static double centreX_;
    private static double centreZ_;
    private static double tiltX_;
    private static double tiltZ_;
    private static boolean calibrated_;
    private static int rotationDir_;
    private static Array2DRowRealMatrix mechanicalCentre;
    private static String zStage = "";
    private static String rStage = "";

    private final Studio studio_;
    private final CMMCore core_;

    public OpticalRotation(Studio studio) {
        studio_ = studio;
        core_ = studio_.getCMMCore();

        StrVector zDrives = core_.getLoadedDevicesOfType(DeviceType.StageDevice);
        boolean error = zDrives.isEmpty() || zDrives.size() < 2;
        if (!error) {
            zStage = zDrives.get(0);
            rStage = zDrives.get(1);
        }

        zStage = studio_.profile().getSettings(OpticalRotation.class).getString(CURRENTZDRIVE, zStage);
        rStage = studio_.profile().getSettings(OpticalRotation.class).getString(CURRENTRDRIVE, rStage);

        centreX_ = studio_.profile().getSettings(OpticalRotation.class).getDouble(CENTREX, 0.0);
        centreZ_ = studio_.profile().getSettings(OpticalRotation.class).getDouble(CENTREZ, 0.0);

        tiltZ_ = studio_.profile().getSettings(OpticalRotation.class).getDouble(TILTZ, 0.0);
        tiltX_ = studio_.profile().getSettings(OpticalRotation.class).getDouble(TILTX, 0.0);

        rotationDir_ = studio_.profile().getSettings(OpticalRotation.class).getInteger(ROTATIONDIR, 1);
        calibrated_ = studio_.profile().getSettings(OpticalRotation.class).getBoolean(CALIBRATED, false);
    }

    public void registerCalibration(double centreZ, double centreX, int rotationDir, boolean calibrated) {
        centreX_ = centreX;
        centreZ_ = centreZ;
        rotationDir_ = rotationDir;
        calibrated_ = calibrated;

        studio_.profile().getSettings(OpticalRotation.class).putDouble(CENTREX, centreX);
        studio_.profile().getSettings(OpticalRotation.class).putDouble(CENTREZ, centreZ);
        studio_.profile().getSettings(OpticalRotation.class).putInteger(ROTATIONDIR, rotationDir);
        studio_.profile().getSettings(OpticalRotation.class).putBoolean(CALIBRATED, calibrated);
    }

    public void registerTilt(double tiltZ, double tiltX) {
        tiltX_ = tiltX;
        tiltZ_ = tiltZ;

        studio_.profile().getSettings(OpticalRotation.class).putDouble(TILTX, tiltX);
        studio_.profile().getSettings(OpticalRotation.class).putDouble(TILTZ, tiltZ);
    }

    public double[] getOpticalCentre() {
        return new double[] { centreZ_, centreX_ };
    }

    public int getRotationDirection() {
        return rotationDir_;
    }

    public boolean getCalibrated() {
        return calibrated_;
    }

    public double[] getCentre() {
        return new double[] { centreZ_, centreX_ };
    }

    public double[] getTilt() {
        return new double[] { tiltZ_, tiltX_ };
    }

    private void translateRotationCompensation(double angleDeg) throws Exception {
        OpticalRotation.this.translateRotationCompensation(angleDeg, getZXCoordinates(), false);
    }

    private void translateRotationCompensation(double angleDeg, double[] origin, boolean relative) throws Exception {
        double[] targetPosition = computeRotationCompensation(angleDeg, origin, relative);
        moveZX(targetPosition, relative);
    }

    private double[] computeRotationCompensation(double angleDeg, double[] origin, boolean relative) {
        if (!calibrated_) {
            if (relative) {
                return new double[] { 0.0, 0.0 };
            } else {
                return origin;
            }
        }

        double radAngle = Math.toRadians(angleDeg);
        radAngle *= rotationDir_;
        mechanicalCentre = new Array2DRowRealMatrix(new double[] { centreZ_, centreX_ });
        Array2DRowRealMatrix rotMat = new Array2DRowRealMatrix(new double[][] {
                { Math.cos(radAngle), -Math.sin(radAngle) }, { Math.sin(radAngle), Math.cos(radAngle) } }, false);

        Array2DRowRealMatrix opticalCentre = new Array2DRowRealMatrix(origin);

        Array2DRowRealMatrix target = opticalCentre.subtract(mechanicalCentre);
        target = rotMat.multiply(target);
        target = target.add(mechanicalCentre);

        if (relative) {
            target = target.subtract(opticalCentre);
        }

        double[] targetPosition = { target.getEntry(0, 0), target.getEntry(1, 0) };
        return targetPosition;
    }

    public void moveZX(double[] position, boolean relative) throws Exception {
        if (relative) {
            core_.waitForDevice(zStage);
            core_.setRelativePosition(zStage, position[0]);
            core_.waitForDevice(zStage);

            core_.waitForDevice(core_.getXYStageDevice());
            core_.setRelativeXYPosition(position[1], 0.0);
            core_.waitForDevice(core_.getXYStageDevice());
        } else {
            core_.waitForDevice(zStage);
            core_.setPosition(zStage, position[0]);
            core_.waitForDevice(zStage);

            core_.waitForDevice(core_.getXYStageDevice());
            core_.setXYPosition(position[1], core_.getYPosition());
            core_.waitForDevice(core_.getXYStageDevice());
        }
    }

    public void moveZ(double um, boolean relative) throws Exception {
        core_.waitForDevice(zStage);

        if (relative) {
            core_.setRelativePosition(zStage, um);
        } else {
            core_.setPosition(zStage, um);
        }
        core_.waitForDevice(zStage);
    }

    public void rotate(double angleDeg) throws Exception {
        rotate(angleDeg, false);
    }

    public void rotate(double angle, boolean radians) throws Exception {
        rotateMotor(angle);
        if (radians) {
            angle = Math.toDegrees(angle);
        }
        translateRotationCompensation(angle);
    }

    public void rotateMotor(double angle) throws Exception {
        core_.waitForDevice(rStage);
        core_.setRelativePosition(rStage, angle);
        core_.waitForDevice(rStage);
    }

    public void rotateMotorAbsolute(double angle) throws Exception {
        core_.waitForDevice(rStage);
        core_.setPosition(rStage, angle);
        core_.waitForDevice(rStage);
    }

    public void rotateAbsolute(double angleDeg) throws Exception {
        rotateAbsolute(angleDeg, false);
    }

    public void rotateAbsolute(double angle, boolean radians) throws Exception {
        double currentAngle = getRPosition(radians);
        rotate(angle - currentAngle, radians);
    }

    public double[] getZXCoordinates() throws Exception {
        double x = core_.getXPosition();
        double z = getZPosition();
        return new double[] { z, x };
    }

    public double getRPosition() throws Exception {
        return getRPosition(false);
    }

    public double getRPosition(boolean radians) throws Exception {
        double angle = core_.getPosition(rStage);
        if (radians) {
            angle = Math.toRadians(angle);
        }
        return angle;
    }

    public double getZPosition() throws Exception {
        return core_.getPosition(zStage);
    }

    public String getZStage() {
        return zStage;
    }

    public String getRStage() {
        return rStage;
    }

    public void setZStage(String stage) {
        zStage = stage;
        studio_.profile().getSettings(OpticalRotation.class).putString(CURRENTZDRIVE, zStage);
    }

    public void setRStage(String stage) {
        rStage = stage;
        studio_.profile().getSettings(OpticalRotation.class).putString(CURRENTRDRIVE, rStage);
    }

    public PolarSystem newPolarSystem() throws Exception {
        return new OpticalRotation.PolarSystem(this);
    }

    public PolarSystem newPolarSystem(double[] rotationCentre) throws Exception {
        return new OpticalRotation.PolarSystem(this, rotationCentre);
    }

    public class PolarSystem {

        private final OpticalRotation rotationManager;
        private final double[] rotationCentre;
        private final double zeroAngle;

        /**
         * Create a manager to move the stage in a polar system of coordinates centred
         * around the current position
         * 
         * @param rotationManager
         * @throws Exception
         */
        public PolarSystem(OpticalRotation rotationManager) throws Exception {
            this.rotationManager = rotationManager;
            this.rotationCentre = rotationManager.getZXCoordinates();
            this.zeroAngle = rotationManager.getRPosition();
        }

        public PolarSystem(OpticalRotation rotationManager, double[] rotationCentre) throws Exception {
            this.rotationManager = rotationManager;
            this.rotationCentre = rotationCentre;
            this.zeroAngle = rotationManager.getRPosition();
        }

        public void moveAbsolute(double radius, double angleDeg) throws Exception {
            angleDeg = zeroAngle + angleDeg;
            double[] target = rotationManager.computeRotationCompensation(angleDeg, rotationCentre, false);
            target[0] += radius;
            rotationManager.moveZX(target, false);
            rotationManager.rotateMotorAbsolute(angleDeg);
        }

        public double getLocalAngle() throws Exception {
            return rotationManager.getRPosition() - zeroAngle;
        }

        /**
         * Move the stage to the centre of the polar system
         * 
         * @throws Exception
         */
        public void centreStage() throws Exception {
            rotationManager.translateRotationCompensation(getLocalAngle(), rotationCentre, false);
        }

        /**
         * Compute the Cartesian ZX coordinates of a given point in the polar space
         * (theta = current angle)
         * 
         * @param radius
         * @return
         * @throws Exception
         */
        public double[] toCartesianZX(double radius) throws Exception {
            double[] coords = getCentreZX();
            coords[0] += radius;
            return coords;
        }

        /**
         * Get the ZX coordinates of the centre of the polar system
         * 
         * @return
         * @throws Exception
         */
        public double[] getCentreZX() throws Exception {
            return rotationManager.computeRotationCompensation(getLocalAngle(), rotationCentre, false);
        }

        /**
         * Rotate the stage to a given angle around the centre of the polar system
         * 
         * @param angleDeg
         * @throws Exception
         */
        public void rotateAbsolute(double angleDeg) throws Exception {
            rotateRelative(angleDeg - getLocalAngle());
        }

        public void rotateRelative(double angleDeg) throws Exception {
            rotationManager.rotateMotor(angleDeg);
            rotationManager.translateRotationCompensation(getLocalAngle(), rotationCentre, true);
        }

    }
}
