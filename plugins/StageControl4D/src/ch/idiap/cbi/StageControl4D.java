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

import org.micromanager.MenuPlugin;
import org.micromanager.Studio;

import org.scijava.plugin.Plugin;
import org.scijava.plugin.SciJavaPlugin;

@Plugin(type = MenuPlugin.class)
public class StageControl4D implements SciJavaPlugin, MenuPlugin {

    private Studio studio_;
    private StageControl4DFrame frame_;

    /**
     * This method receives the Studio object, which is the gateway to the
     * Micro-Manager API. You should retain a reference to this object for the
     * lifetime of your plugin. This method should not do anything except for store
     * that reference, as Micro-Manager is still busy starting up at the time that
     * this is called.
     */
    @Override
    public void setContext(Studio studio) {
        studio_ = studio;
    }

    /**
     * This method is called when your plugin is selected from the Plugins menu.
     * Typically at this time you should show a GUI (graphical user interface) for
     * your plugin.
     */
    @Override
    public void onPluginSelected() {
        if (frame_ == null) {
            frame_ = new StageControl4DFrame(studio_);
            studio_.events().registerForEvents(frame_);
            frame_.initialize();
        }
        frame_.setVisible(true);
    }

    /**
     * This string is the sub-menu that the plugin will be displayed in, in the
     * Plugins menu.
     */
    @Override
    public String getSubMenu() {
        return "";
    }

    /**
     * The name of the plugin in the Plugins menu.
     */
    @Override
    public String getName() {
        return "4D stage controller";
    }

    @Override
    public String getHelpText() {
        return "GUI to control a 4D stage.";
    }

    @Override
    public String getVersion() {
        return "1.0";
    }

    @Override
    public String getCopyright() {
        return "Idiap Research Institute, 2018";
    }
}
