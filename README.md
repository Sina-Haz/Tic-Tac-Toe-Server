
# Multithreaded Tic Tac Toe
### Our Approach:
 - We decided to use a helper method and header file to break down messages received by read to ensure we don’t read in too much or too little data
 - We then put the input string that we received into a function that would convert it into a tic-tac-toe command like the one’s given in the assignment description

### Testing
The way we tested this program was by playing several games, ensuring the server could do the following:
1. Maintain a connection for a complete game
	To do this, we would set up a game between two players and play until there was a winner.<br>
	The server was able to successfully keep track of the board and display the proper MOVD commands after every move.
2. Handle Multithreading
-	The server was able to handle numerous games simultaneously, keeping track of the boards in every game
3. Handle Draws:
-	We tested multiple different draw scenarios


Players reach 9 total moves without a winner (board fills up)
One player suggests a draw and the other accepts it
One player suggests a draw and the other rejects it, allowing the player who suggested the draw to still make his regular move command

### Various Test Inputs:
- NAME|10|Chris Hall| (successful)
- MOVE|6|X|2,2| (successful)
- RAW|2|S| (Throws Error)

### Example Game:
Client 1: NAME|10|Chris Hall|
Client 2: PLAY|6|Bryan|
Client 1: MOVE|6|X|1,1|
Client 2: MOVD|16|X|1,1|X……..|
	MOVE|6|O|3,3|
Client 1: MOVD|16|O|3,3|X…….O|
	MOVE|6|X|2,1|
Client 2: MOVD|16|X|2,1|XX……O|
	MOVE|6|O|2,3|
Client 1: MOVD|16|O|XX…..OO|
	MOVE|6|X|3,1|
PLAYER 1 WINS
