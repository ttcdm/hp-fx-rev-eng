
        HP VISUALIZE fx5, fx10 OpenGL Graphics Accelerator Board
           (HP Visualize Personal Workstation - Windows 2000)


    In This Document

       * Pre Driver Installation/Update Steps for Windows 2000
       * First Time Driver Installation Procedure for Windows 2000
       * Driver Update Procedure for Windows 2000
       * Post Driver Installation Steps for Windows 2000
          - Verify that VisualizeFX is the Primary Monitor
          - Disable Mouse Pointer Shadows
       * Driver Configuration
       * Further Information
       * Visualize Diagnostics
       * 3D StudioMAX Driver


    Pre Driver Installation/Update Steps for Windows 2000

      As a first step, you need to unpack the driver files:
      
       1. Transfer the file you downloaded (e.g. fx_104.exe) to an 
          empty directory on your hard disk (e.g. c:\temp\visfx\).
       2. Double-click the filename to decompress the files 
          contained in the self extracting archive.
       3. Now you are ready to install or update your drivers
	  as described below.


    First Time Driver Installation Procedure for Windows 2000

      Follow these steps if the Found New Hardware Wizard starts when you 
      first login after installing your HP Visualize fx card.

       1. When the "Found New Hardware Wizard" window opens, click Next
          to continue. 
       2. Select "Display a list of known drivers for this device" 
          and then click Next.
       3. Click the Have Disk button and browse to the directory that
          contains the HP Visualize fx driver.
       4. Select the hpfxlw2k.inf file. Click Open and then OK.
       5. This should return you to the "Select a Device Driver" 
          window and "Hewlett-Packard VisualizeFX Series Video" 
          should be highlighted in the list of models. Click Next.
       6. If you get a Digital Signature Not Found warning, click Yes 
	  to continue.
       7. Click Finish to complete the driver update.
       8. Now you must restart the system for the new drivers 
          to take effect.
       9. Follow the Post Driver Installation Steps described below
          after the system has restarted. 
       
      NOTE: if the "Found New Hardware Wizard" does not automatically start, 
      you can manually start it by right clicking the My Computer Icon, 
      selecting Properties, and then clicking the Hardware Wizard button. 


    Driver Update Procedure for Windows 2000

      Follow these steps if you are updating the existing fx driver to a
      newer version:

       1. Right click the My Computer icon, and select Properties.
       2. Click on the Hardware tab and select the Device Manager box.
       3. If there is a Video Controller (with a question mark) under 
          the Other Devices icon, and the Display Adapters icon only shows 
          the Matrox Graphics Millenium G200, go to step 4a. Else, go to 
          step 4b.
       4a. Double click the Video Controller (with the question mark) under 
           the Other Devices icon. Go to step 5.    
       4b. Click on the Display Adapters icon, then double click on 
           Hewlett-Packard VisualizeFX Series Video. Go to step 5.
       5. Select the Driver tab and then click the Update Driver box.
       6. This should start the "Upgrade Device Driver Wizard".
          Click Next.
       7. Select "Display a list of known drivers for this device" 
          and then click Next.
       8. Click the Have Disk button and browse to the directory that
          contains the HP Visualize fx driver.
       9. Select the hpfxlw2k.inf file. Click Open and then OK.
      10. This should return you to the Upgrade Device Driver Wizard 
          and "Hewlett-Packard VisualizeFX Series Video" should be
          highlighted in the list of models. Click Next.
      11. If you get a Digital Signature Not Found warning, click Yes 
	  to continue.
      12. Click Finish to complete the driver update.
      13. Close the Properties window.
      14. Now you must restart the system for the new drivers 
          to take effect.
      15. If this is the first time an fx driver has been installed, 
          follow the Post Driver Installation Steps described below 
          after the system has restarted.
       
   

    Post Driver Installation Steps for Windows 2000

      1. Verify that VisualizeFX is the Primary Monitor

         a. Start the Control Panel: Click Start, point to Settings
            and select Control Panel.
         b. Double-click on Display and click the Settings tab.
         c. If Hewlett-Packard VisualizeFX Series Video is the
            only display listed, you are done. 
         d. If two displays are listed (Matrox Graphics Millennium G200
            and Hewlett-Packard VisualizeFX Series Video), follow the
            remaining steps to extend the desktop to VisualizeFX.

            NOTE: The Matrox device is part of the VisualizeFX card.
            It provides VGA functionality and cannot be used at the 
            same time while using VisualizeFX graphics. Thus, you 
            should not extend the desktop to the Matrox device.

         e. Select VisualizeFX in the list of displays.  
            Check 'Extend my Windows desktop onto the monitor' 
            and 'Use this device as the primary monitor'
         f. Select Matrox Graphics in the list of displays.  
            Uncheck 'Extend my Windows desktop onto the monitor'
         g. Click the apply button.

      2. Disable Mouse Pointer Shadows

         The default Windows 2000 mouse pointer may cause problems
         between the pointer and OpenGL applications. Follow these
         steps to avoid these problems:
         
         a. Start the Control Panel: Click Start, point to Settings
            and select Control Panel.
         b. Double-click on Mouse and click the Pointers tab.
         c. Uncheck 'Enable pointer shadow'


    Driver Configuration

    You should optimize the driver settings to suit the OpenGL 
    application that you are using on your system. This can be 
    done through the driver applet which is installed automatically 
    with the driver.

      1. From the Start menu, click "Settings" then "Control Panel".
      2. Double-click the "Display" icon, click on the Advanced button
         and select the "Options" tab.
      3. Select your application in the "Application" list box. 
         Driver settings (viewed on the right) will be configured 
         to best suit your application. If the application you are 
         using is not listed, select "Compatibility Mode".
      4. Click the "Apply" button, then "OK".

    For explanations of each setting, click on the question mark (?) 
    in the upper right hand corner of the window and then click on 
    the option for which you want to see an explanation.


    Further Information

    For information on the advanced configuration options of the 
    HP Visualize fx graphics board as well as detailed troubleshooting
    steps, please refer to the screen-viewable PDF file 
    "HP Visualize fx Configuration Guide".

    This manual is available:

       * Preloaded on the hard disk of your PC Workstation: 
         Click Start, Programs, HP Info, 
         HP Visualize fx Configuration Guide
       * Supplied on the HP Driver CD-ROM
       * Available from the HP visualize support website
         http://www.hp.com/workstations/support


    Visualize Diagnostics

    A diagnostics utility is available to test your VisualizeFX
    graphics board. If you suspect that your board may have a fault, 
    you can use this utility to run a thorough hardware test.

    This utility is available:

       * Preloaded on the hard disk of your PC Workstation:
         Click Start, Programs, HP Diagnostics, 
         HP Visualize fx Diagnostics
       * Supplied on the HP Driver CD-ROM
       * Available from the HP visualize support website
         http://www.hp.com/workstations/support

 
    3D StudioMAX version 3 Driver

    Copy the hpoglgfx.drv file to the 3D StudioMAX 3 home directory. 
    (This should be the same directory as the oglgfx.drv file).

    Start 3D StudioMAX, and select the Customize->Preferences menu 
    item. Select the Viewports tab, and select the Choose Driver 
    button in the lower right corner. Next to the Custom button, 
    you should see a drop down selector.  Select the hpoglgfx.drv 
    custom driver and click "OK". Now select the Configure Driver
    button and configure the driver as follows:

      OFF: Redraw Scene on Window Expose
      ON:  Use Triangle Strips
      ON:  Display Wireframe Objects Using Triangle Strips
      ON:  Allow Dual Plane Support
      ON:  Use Incremental Scene Updates
      ON:  Use BGRA pixel format
      ON:  Use Generalized Vertex Arrays

    You will need to restart 3D StudioMAX for the changes to
    take effect.

