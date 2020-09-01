section "BOOT", rom0[$0000]

; Setup stack
    ld sp, $fffe

; Zero video ram ($8000-$9FFF)
    xor a
    ld hl, $9fff
clear_vram:
    ld [hl-], a
    bit 7, h
    jr nz, clear_vram

; Initialize sound
    ld a, $80
    ldh [$11], a    ; $ff11: square 0 duty/length
    ldh [$26], a    ; $ff26: square 1 duty/length
    ld a, $f3
    ldh [$12], a    ; $ff12: square 0 volume
    ldh [$25], a    ; $ff25: enable output
    ld a, $77
    ldh [$24], a    ; $ff24: volume to 100%

; Set BG palette
    ld a, $fc
    ldh [$47], a    ; $ff47: bg palette

; Load logo data from the ROM
    ld de, $0104    ; source address in ROM
    ld hl, $8010    ; destination address in VRAM

; Logo data is stored as 4x4 tiles that are scaled up to 8x8 by doubling pixels
; Only one plane of tile data is used (1-bit color)
; Each row fits into 4 bits, 2 bytes for the entire tile
unpack_logo:
    ld a, [de]      ; reads 2 rows of data
    ld c, a
    call write_doubled_tile_row
    call write_doubled_tile_row
    inc de
    ld a, e
    cp $34          ; keep going for 48 bytes
    jr nz, unpack_logo

; Load in extra tile for trademark glyph (stored in this BIOS)
    ld de, trademark_tile_data
    ld b, 8         ; 8 bytes/rows for 1 tile
load_trademark_tile_row:
    ld a, [de]      ; load a row of pixels
    inc de
    ld [hl+], a     ; store into VRAM
    inc hl          ; skip over second color plane
    dec b
    jr nz, load_trademark_tile_row

; Now position all the logo tiles within the tile map
    ld a, $19       ; tile index for trademark
    ld [$9910], a   ; position the trademark tile
    ld hl, $992f    ; start loading in logo tiles here and work backwards
    ld c, 12        ; 12 tiles per row
load_logo_tilemap:
    dec a           ; select next tile index
    jr z, finished_logo_tilemap
    ld [hl-], a     ; write tile index to VRAM tile map
    dec c           ; move left to next column
    jr nz, load_logo_tilemap
    ld l, $0f       ; now load next row (top) of the tile map
    jr load_logo_tilemap
finished_logo_tilemap:

; Now that BG tile set and map are loaded, scroll the logo
; onto the screen. This is intended to be a faster animation
; than in the original DMG BIOS.
    ld h, a         ; use h as scroll counter (h=0)
    ;ld a, $64
    ld a, 0
    ldh [$42], a    ; $ff42: set ScrollY=66 so logo is outside top of screen
    ld a, $91
    ldh [$40], a    ; $ff40: enable BG, use tileset $8000, power up LCD

    ld c, 60
    call wait_frames ; wait for a second

; Match original bios register values
    ld hl, $01b0
    push hl
    pop af
    ld bc, $0014
    ld de, $00d8
    ld hl, $014d

    jp exit_bios

; Doubles the 4 most significant bits in 'c' to fill two rows
write_doubled_tile_row:
    ld b, 0
    ld a, 4         ; each row has 4 bits
; Doubles the bits in the row
unpack_bits:
    sla c           ; load bit into carry
    push af         ; preserve carry flag
    rl b            ; loads bit into b
    pop af          ; re-use the carry flag
    rl b            ; rolls same bit into b again
    dec a           ; onto the next bit
    jr nz, unpack_bits

; Store the row data into VRAM
; Note: Second "plane" of tile data is skipped over
    ld a, b
    ldi [hl], a     ; first row
    inc hl          ; skip second plane
    ldi [hl], a     ; second row
    inc hl          ; skip second plane again
    ret

; Waits for the PPU to reach VBLANK and raise the IRQ
wait_vblank:
    push hl
    ld hl, $ff0f
    res 0, [hl]      ; unset VBLANK IRQ flag
.loop
    bit 0, [hl]     ; wait until VBLANK IRQ is set again
    jr z, .loop
    pop hl
    ret

; Waits for 'c' #frames
wait_frames:
    call wait_vblank
    dec c
    jr nz, wait_frames
    ret

trademark_tile_data: ; stores data for (R) glyph
db $3c,$42,$b9,$a5,$b9,$a5,$42,$3c

section "ExitBios", rom0[$00fe]
exit_bios:
    ldh [$50], a     ; unmaps BIOS from memory
; Entering into cartridge ROM @ $0100