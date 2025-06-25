.section .data

.global tim_my_image
.type tim_my_image, @object
tim_my_image:
    .incbin "assets/my_image.tim"

.global tim_background
.type tim_background, @object
tim_background:
    .incbin "assets/background.tim"