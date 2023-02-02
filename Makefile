all: sysprak-client

.PHONY: all clean play play-valgrind play-new play-new-valgrind test

clean:
	rm -rf bin build

sysprak-client: $(wildcard src/*.c) $(wildcard src/*.h)
	gcc -Wall -Wextra -Werror -g -o sysprak-client src/*.c

play: sysprak-client
	./sysprak-client -g $$GAME_ID -p $$PLAYER

play-valgrind: sysprak-client
	valgrind --leak-check=full --show-leak-kinds=all --trace-children=yes --track-origins=yes ./sysprak-client -g $$GAME_ID -p $$PLAYER

play-new: sysprak-client
	@GAME_ID=$$(curl http://sysprak.priv.lab.nm.ifi.lmu.de/api/v1/matches \
	-H "Content-Type: application/json" \
	-X POST \
	-d '{"type":"'Quarto'","gameGeneric":{"name":"","timeout":3000},"gameSpecific":{},"players":[{"name":"Player 1","type":"COMPUTER"},{"name":"Player 2","type":"HUMAN"}]}' 2>/dev/null | grep -Eow '([a-z0-9]{13})'); \
	echo "Generated new game with ID \"$$GAME_ID\"."; \
	xdg-open http://sysprak.priv.lab.nm.ifi.lmu.de/quarto/\#$$GAME_ID; \
	./sysprak-client -g $$GAME_ID -p 1

play-new-valgrind: sysprak-client
	@GAME_ID=$$(curl http://sysprak.priv.lab.nm.ifi.lmu.de/api/v1/matches \
	-H "Content-Type: application/json" \
	-X POST \
	-d '{"type":"'Quarto'","gameGeneric":{"name":"","timeout":3000},"gameSpecific":{},"players":[{"name":"Player 1","type":"COMPUTER"},{"name":"Player 2","type":"HUMAN"}]}' 2>/dev/null | grep -Eow '([a-z0-9]{13})'); \
	echo "Generated new game with ID \"$$GAME_ID\"."; \
	xdg-open http://sysprak.priv.lab.nm.ifi.lmu.de/quarto/\#$$GAME_ID; \
	valgrind --leak-check=full --show-leak-kinds=all --trace-children=yes --track-origins=yes ./sysprak-client -g $$GAME_ID -p 1 client.conf

build/abgabe.zip: $(wildcard src/**) Makefile
	@mkdir -p build
	@rm -f build/abgabe.zip
	zip -o build/abgabe.zip src/** Makefile
	@touch build/abgabe.zip # set modify date to now

test: build/abgabe.zip
	rm -rf build/test
	./test-script.sh build/test valgrind.log build/abgabe.zip --spectate
