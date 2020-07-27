# Melody
MELODY is a digital-physical device that enables the creation of collaborative short musical jams


Alongside the many advantages and technological solutions that allow work from home,
the difficulty of formulating and creating life support among co-workers remains. 
MELODY is a digital-physical device that enables the creation of collaborative short musical jams. 
Co-workers coordinate time and the device set a jam session with turns and different random sounds. The first participant sets a specific rhythm, 
after which each participant adds their own musical section corresponding to the set rhythm. In order to make it easier for users with no musical background,
the software helps them keep pace by sampling their clicks and adjusting to the appropriate rhythm.
The session ends after about 3 minutes when all participants have finished recording their part.

How does it work?
Melody is based on ESP2866 hardware, which communicates with a Node-Red server over MQTT protocol.
The device translates the player’s notes into a string of characters that is sent to the server and from the server back to the other players. 
This allows everyone to play and hear the tune without interruption from their network connection.
Melody has two main visual indicators. The first is a LED strip that lets the player know when Loop starts and when it ends and indicates if it’s the player’s turn. 
The second is a LED display in the center of the product, which is used to visually display the existing tune. 
A countdown from 3 to 1 indicates to start playing and a timings display directs the user when and how she wants to contribute to the group’s Melody. 
The recording is automatically saved to the company’s cloud for future use.

http://milab.idc.ac.il/teaching/projects/melody/
