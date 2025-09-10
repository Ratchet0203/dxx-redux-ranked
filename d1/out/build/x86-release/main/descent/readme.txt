
                     ษอออออออออออออออออออออออออออป
                     บ     RELEASE NOTES FOR     บ
                     บ          DESCENT          บ
                     บ REGISTERED/COMMERCIAL 1.0 บ
                     ศอออออออออออออออออออออออออออผ

            ฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤ
                       PLEASE DO NOT DISTRIBUTE!
               REGISTERED DESCENT IS A COMMERCIAL PRODUCT

                       See section 8 for details
            ฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤ


                         ษอออออออออออออออออออป
                         บ TABLE of CONTENTS บ
                         ศอออออออออออออออออออผ

                      1. Version Changes

                      2. Manual Addenda

                      3. Tips for New Pilots

                      4. Using Thrustmaster controls, Logitech
                         Wingman Extreme, Gravis Phoenix,
                         or Logitech Cyberman

                      5. VR Helmet Information

                      6. Troubleshooting & Technical Support



                         ษออออออออออออออออออออป
                         บ 1. VERSION CHANGES บ
                         ศออออออออออออออออออออผ

For Shareware owners, the following items have been fixed/added since
Descent Shareware 1.1:

     Fixed: memory allocation problems in mouse.c
            sudden loss of joystick calibration
            UART detection problems
            lockups with certain TI/Cyrix chips
            lockups with PS/2 style mice

     Added: enhanced FM and MIDI soundtrack
            support for .MSN mission files
            improved enemy AI
            improved automap controls
            more fullscreen HUD info
            network support across routers
            support external controls for SpaceBall
            Descent FAQ 1.0


                          ษอออออออออออออออออออป
                          บ 2. MANUAL ADDENDA บ
                          ศอออออออออออออออออออผ

The following items were added after the game manual went to print:

DESCENT and OS/2
ฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤ
To get Descent to run under OS/2, just increase your DPMI_MEMORY_LIMIT
to 8 megs or more.  Descent should then run perfectly with full sound.
If your joystick behaves strangely under OS/2 try using the "-JoyPolled"
command line option.  Keep in mind that Descent does not officially
support OS/2.


DESCENT and Windows
ฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤ
Use the following settings in your PIF file:

   Optional Parameters:  -JoyPolled
            EMS Memory:  KB Limit -1
            XMS Memory:  KB Limit -1
         Display Usage:  Full Screen
             Execution:  Exclusive

If you use a joystick, the -JoyPolled option uses an alternate method of
joystick reading that should increase performance under Windows.
Keep in mind that Descent does not officially support Windows.


DESCENT and INTERNET HEAD-TO-HEAD DEMON (IHHD)
ฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤ
While playing Multiplayer Descent through the IHHD software is possible,
it is not recommended.  The delay in packet delivery will almost
certainly lead to strange results.  You may see players jump around
suddenly, or even disappear for several seconds at a time.  You may also
witness players not dropping powerups when killed, staying cloaked too
long, or other unfavorable results.


COMMAND LINE OPTIONS
ฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤ
The following command-line options were not mentioned in the manual:

  -RInvul <n>    New multiplayer option.  Keeps the Main Reactor
                 invulnerable until n minutes have passed where n is a
                 value between 1-314.

  -AutoDemo      Continuously cycles through all DEM files in directory.

  -NoMusic       Disables music; sound effects remain enabled.

  -JoyPolled     Reads joystick using the standard IBM PC polling method,
                 instead of using the 1.19Mhz system timer.  This is useful
                 for multitasking operating systems, like OS/2 and Windows,
                 which support the polling method.

  -JoyBios       Reads joystick using BIOS function 15h.  This will
                 improve the joystick readings under certain operating
                 systems and configurations.  This is not normally
                 recommended for DOS users.


SCREENSHOT VIEWER
ฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤ
Included in your Descent directory is PCXVIEW.EXE.  This is a standalone
utility that allows you to view any .PCX screenshots taken in Descent.
PCXVIEW.EXE is public domain and freely distributable.  See your Descent
Manual for more information on screenshots.


                       ษออออออออออออออออออออออออป
                       บ 3. TIPS for NEW PILOTS บ
                       ศออออออออออออออออออออออออผ

In addition to the following tips, your Descent box includes a "walkthrough"
for level 1.  It explains in detail how to maneuver your way through level
1 and explains the basics of gameplay.

Here are a few things that help new players adjust to the fully 3D world
and intelligent robots of Descent.  Heed these tips and you'll soon be
blasting robot hordes (and your friends!) with ease.

     o Use a quality joystick.  Thrustmaster, Advanced Gravis, and
       CH Products provide full lines of controls well suited to 3D
       flying.

     o If you use a two-button joystick, try keeping one hand on the
       A and Z keys to control your FORWARD and REVERSE.

     o If you're up against a wall, try using REVERSE to back away 
       instead of turning around.

     o Learn how to SLIDE well.  Using vertical sliding instead of pitching
       will help keep you upright.  If your joystick has a Hat Switch, 
       use it to slide.

     o Use SLIDE to evade enemy fire.  This will help keep the robots
       in your sights.

     o Sweep fire side-to-side to hit evading robots.  This is especially
       important on higher skill levels where robots evade faster.  Also
       try using lasers and missiles simultaneouslly against an evading
       robot.

     o Rescue the hostages!  If you rescue all hostages in a mine, you
       receive a full rescue bonus.  But be sure you make it out after
       picking them up - if your ship's destroyed, so are they.

     o Try clearing a path to the exit before you attack the Main Reactor.
       You may run out of time if you're battling hordes of robots on
       your escape!

     o The blast radius of a concussion missile is very effective 
       against tight groups of robots.  

     o Look for secret doors.  Weapons and ammo are often behind them.                      
       They can be a different color, or have small seams running down
       them.  Keep in mind, Descent is fully 3D, and secret doors aren't
       only in the walls...

     o DON'T SIT STILL.  The best way to stay alive is to keep moving.
       Using SLIDE to circle a target while you're firing is very
       effective.

     o Learn to use your ship's Automap.  It can reveal things your
       eyes may miss.

     o Flares can help sniff out cloaked robots & players.


               ษออออออออออออออออออออออออออออออออออออออออออออป
               บ 4. USING THRUSTMASTER CONTROLS, LOGITECH   บ
               บ     WINGMAN EXTREME, GRAVIS PHOENIX and    บ
               บ             LOGITECH CYBERMAN              บ
               ศออออออออออออออออออออออออออออออออออออออออออออผ

Thrustmaster, Gravis Phoenix, and Logitech Cyberman controls are fully
supported by Descent.  The device control files (DESCENT2.ADV, DESCENT.B50,
DESCENT.M50, and DESCENT.PHX) can be found in your Descent directory.
If you have a Wingman Extreme, you should use the Thrustmaster FCS option.

       Thrustmaster FCS:  Choose "Thrustmaster FCS" under Options/Controls.
         or Logitech      This is for players _not_ also using the WCS.
        Wingman Extreme   If you use the WCS throttle with the FCS, follow
                          instructions for the WCS below.

     * Thrustmaster WCS:  Load the DESCENT2.ADV file into your WCS.  In
           Mark II        Descent, choose "Joystick" under Options/Controls.
                          Now choose "Configure Above" and change the
                          THROTTLE entry to respond to the WCS (the Y2 axis).

    * Thrustmaster FLCS:  Load the DESCENT.B50 & .M50 files into your FLCS. 
                          If you also use a WCS throttle, no program should
                          be loaded into the WCS.

         Gravis Phoenix:  Load the DESCENT.PHX file into your Phoenix. In
                          Descent, choose "Joystick" under Options/Controls.
                          Now choose "Configure Above" and change the
                          THROTTLE entry to respond to the throttle
                          slider (the Y2 axis).

      Logitech Cyberman:  You must have the Cyberman MOUSE.COM driver loaded
                          before you run Descent.  If you have a standard
                          mouse connected as well, you will need to use the
                          "/dual" option when you load MOUSE.COM.  See
                          Cyberman docs for details.  

Calibration Note:  When using one of the analog throttles listed here or
                   the CH Flightstick, you will be prompted to calibrate
                   your throttle as well as your joystick.

*  >> NOTE TO WCS and FLCS USERS:  Because these devices intercept the
      keyboard signals, your ship may fly strangely when you also hold down
      keyboard keys (like 'A' and 'Z').  If you keep the WCS or FLCS on
      your system, it's recommended that you use those devices for throttle
      control and sliding, not the keyboard.


                       
                 ษออออออออออออออออออออออออออออออออออออออป
                 บ 5. TROUBLESHOOTING & TECHNICAL SUPPORT
                 ศออออออออออออออออออออออออออออออออออออออผ

For the support for all your games bought through GOG.com, please visit our Support page at http://www.gog.com/support. 
If you're logged in, go directly to "Support" page, where you can see a list of all your Good Old Games, otherwise you can find the game you're looking for through the smart search. Choose the game that you have problem with, and see whether the solution isn't already posted. If not, go to the "Contact us" page, select "Technical issues with games" and hit "Continue" to send us a message describing your problem. Fill all the required fields with proper data - please enter as much details as you can, this will help us solve your problem faster. All your messages and our replies will appear on your "My Account" page. 




ฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤ
     DESCENT IS COPYRIGHT (C) 1994, 1995 PARALLAX SOFTWARE CORPORATION
                          ALL RIGHTS RESERVED
          DESCENT IS A TRADEMARK OF INTERPLAY PRODUCTIONS, INC.
ฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤ

-END OF FILE-
