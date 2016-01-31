all: rungame

rungame: game3D.cpp glad.c
	g++ -o game game3D.cpp glad.c -lGL -lglfw -lftgl -lSOIL -ldl -I/usr/local/include -I/usr/include/freetype2 -L/usr/local/lib/ -L/usr/lib -lsfml-audio

clean:
	rm game
