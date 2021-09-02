name=VGA_Terminal
ext=c
port=COM3

send: compile
	# Workaround for WSL2, call avrdude directly if you are using native Linux
	cmd.exe /c "avrdude -F -V -c arduino -p ATMEGA328P -P $(port) -b 115200 -U flash:w:$(name).hex"

compile: $(name).$(ext)
	avr-gcc -O2 -DF_CPU=16000000UL -mmcu=atmega328p -o $(name) $(name).$(ext)
	avr-objcopy -O ihex -R .eeprom $(name) $(name).hex

clean:
	rm -f $(name) $(name).hex
