# WM9M4AssignmentChatRoom5749205
## Current State of the Chat Room
### Login Screen
<img width="821" height="180" alt="Screenshot 2026-02-10 151235" src="https://github.com/user-attachments/assets/12f2f284-1c2d-4571-afde-89f478caa469"/>
<img width="1919" height="1127" alt="Screenshot 2026-02-10 152942" src="https://github.com/user-attachments/assets/761f3def-b5a8-4e12-b74f-1419702ff898" />

*Note 1: Username cannot be empty string (to prevent attacks), or cannot contain any whitespaces*

*Note 2: Users will not see their own client names under Users tab (while the other clients can!), to prevent them from sending direct messages to themselves*

### Broadcasting Between Clients
<img width="918" height="637" alt="Screenshot 2026-02-10 151354" src="https://github.com/user-attachments/assets/4bfcb0ef-8098-4283-a771-e298aebd782c"/>

*Note 1: Users will not see their own client names under Users tab (while the other clients can!), to prevent them from sending direct messages to themselves*

*Note 2: Users can gracefully disconnect from the ChatRoom by typing "/exit" to the broadcast chat (will update this commit once I resolve how to disconnect a client after shutting down from the Windows app or CTRL-C on client console)*

### Direct Message Between Two Clients
<img width="1791" height="831" alt="Screenshot 2026-02-10 151505" src="https://github.com/user-attachments/assets/054128ca-1de0-4e80-9f02-d1c103fc731a"/>
<img width="1918" height="649" alt="Screenshot 2026-02-10 153602" src="https://github.com/user-attachments/assets/b1c122a3-9d7f-4021-bc2f-3323fe64ab22" />

### Server Logs for the Test Tun
<img width="1734" height="927" alt="Screenshot 2026-02-10 153718" src="https://github.com/user-attachments/assets/a25d4201-617f-4a00-91b6-77aa28c7d1d5" />
