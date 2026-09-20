SENTINELRFID SECURITY, by Andres Robles:
This project is a Wokwi-based system that runs by a simulated ESP32 to MFRC522 connection.

Problem: Organizations using RFIDaccess systems may have difficulty testing how their systems respond to cloned credentials by means of card skimming, for example.
Such attacks copy the Unique Identifier of legitimate cards of employees or officials in order to get access to  restricted areas or systems.
Solution: this virtual RFID simulation security lab simulates the tag/card, reader, and pairs UID authentication with a secret key system to catch cloned cards. 
The secret keys in the real world would be linked to specific data blocks stored on the card, which can't easily be replicated by read/write devices due to their ecrypted nature.
This lab focuses on the proof of concept for this (no encryption) in order to show that a simulated clone card attack can be detected by its lack of a secret key.
I was inspired to create this project by my interest in embedded systems and hardware security. It was quite a challenge to write a lot of C, so I relied on AI tools such as Google Gemini 
to help with the debugging process. 

Here is a link for the Wokwi page where the system can be tested.
(https://wokwi.com/projects/475610042227585025) (I just changed this link to the wokwi page)
