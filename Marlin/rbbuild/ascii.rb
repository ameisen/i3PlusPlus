$Ascii_Cls = Class.new do
	@charmap

	def encode(char)
		return char.chr.force_encoding("iso-8859-1");
	end
	
	def initialize ()
		@charmap = {
			"Ç" => encode(0x80),
			"ü" => encode(0x81),
			"é" => encode(0x82),
			"â" => encode(0x83),
			"ä" => encode(0x84),
			"à" => encode(0x85),
			"å" => encode(0x86),
			"ç" => encode(0x87),
			"ê" => encode(0x88),
			"ë" => encode(0x89),
			"è" => encode(0x8A),
			"ï" => encode(0x8B),
			"î" => encode(0x8C),
			"ì" => encode(0x8D),
			"Ä" => encode(0x8E),
			"Å" => encode(0x8F),
			"É" => encode(0x90),
			"æ" => encode(0x91),
			"Æ" => encode(0x92),
			"ô" => encode(0x93),
			"ö" => encode(0x94),
			"ò" => encode(0x95),
			"û" => encode(0x96),
			"ù" => encode(0x97),
			"ÿ" => encode(0x98),
			"Ö" => encode(0x99),
			"Ü" => encode(0x9A),
			"¢" => encode(0x9B),
			"£" => encode(0x9C),
			"¥" => encode(0x9D),
			"₧" => encode(0x9E),
			"ƒ" => encode(0x9F),
			"á" => encode(0xA0),
			"í" => encode(0xA1),
			"ó" => encode(0xA2),
			"ú" => encode(0xA3),
			"ñ" => encode(0xA4),
			"Ñ" => encode(0xA5),
			"ª" => encode(0xA6),
			"º" => encode(0xA7),
			"¿" => encode(0xA8),
			"⌐" => encode(0xA9),
			"¬" => encode(0xAA),
			"½" => encode(0xAB),
			"¼" => encode(0xAC),
			"¡" => encode(0xAD),
			"«" => encode(0xAE),
			"»" => encode(0xAF),
			"░" => encode(0xB0),
			"▒" => encode(0xB1),
			"▓" => encode(0xB2),
			"│" => encode(0xB3),
			"┤" => encode(0xB4),
			"╡" => encode(0xB5),
			"╢" => encode(0xB6),
			"╖" => encode(0xB7),
			"╕" => encode(0xB8),
			"╣" => encode(0xB9),
			"║" => encode(0xBA),
			"╗" => encode(0xBB),
			"╝" => encode(0xBC),
			"╜" => encode(0xBD),
			"╛" => encode(0xBE),
			"┐" => encode(0xBF),
			"└" => encode(0xC0),
			"┴" => encode(0xC1),
			"┬" => encode(0xC2),
			"├" => encode(0xC3),
			"─" => encode(0xC4),
			"┼" => encode(0xC5),
			"╞" => encode(0xC6),
			"╟" => encode(0xC7),
			"╚" => encode(0xC8),
			"╔" => encode(0xC9),
			"╩" => encode(0xCA),
			"╦" => encode(0xCB),
			"╠" => encode(0xCC),
			"═" => encode(0xCD),
			"╬" => encode(0xCE),
			"╧" => encode(0xCF),
			"╨" => encode(0xD0),
			"╤" => encode(0xD1),
			"╥" => encode(0xD2),
			"╙" => encode(0xD3),
			"╘" => encode(0xD4),
			"╒" => encode(0xD5),
			"╓" => encode(0xD6),
			"╫" => encode(0xD7),
			"╪" => encode(0xD8),
			"┘" => encode(0xD9),
			"┌" => encode(0xDA),
			"█" => encode(0xDB),
			"▄" => encode(0xDC),
			"▌" => encode(0xDD),
			"▐" => encode(0xDE),
			"▀" => encode(0xDF),
			"α" => encode(0xE0),
			"ß" => encode(0xE1),
			"Γ" => encode(0xE2),
			"π" => encode(0xE3),
			"Σ" => encode(0xE4),
			"σ" => encode(0xE5),
			"µ" => encode(0xE6),
			"τ" => encode(0xE7),
			"Φ" => encode(0xE8),
			"Θ" => encode(0xE9),
			"Ω" => encode(0xEA),
			"δ" => encode(0xEB),
			"∞" => encode(0xEC),
			"φ" => encode(0xED),
			"ε" => encode(0xEE),
			"∩" => encode(0xEF),
			"≡" => encode(0xF0),
			"±" => encode(0xF1),
			"≥" => encode(0xF2),
			"≤" => encode(0xF3),
			"⌠" => encode(0xF4),
			"⌡" => encode(0xF5),
			"÷" => encode(0xF6),
			"≈" => encode(0xF7),
			"°" => encode(0xF8),
			"∙" => encode(0xF9),
			"·" => encode(0xFA),
			"√" => encode(0xFB),
			"ⁿ" => encode(0xFC),
			"²" => encode(0xFD),
			"■" => encode(0xFE),
			" " => encode(0xFF)
		}
	end
	
	def [] (char)
		return @charmap[char]
	end
end
$Ascii = $Ascii_Cls.new