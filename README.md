# psx-asteroids
Play ASTEROIDS on the PlayStation 1!

Sorry for the messy file structure! I wrote this on my Steam Deck on a vacation, so I didn't also want to have to configure everything with the on screen keyboard. I did write some of the later code on my PC though!

![psx-asteroids](https://github.com/user-attachments/assets/45efaa1c-b3a3-4900-887e-414f2c68ad5a)

To actually compile, you need to install nugget + psyq to the project folder. You need git installed for this to work. To do this, you need to:

1 Run:
```cmd
git init
```

2. Add the submodules:
```cmd
git submodule add https://github.com/pcsx-redux/nugget.git third_party/nugget
git submodule add https://github.com/johnbaumann/psyq_include_what_you_use.git third_party/psyq-iwyu
```

Then, you need to download the PSYQ converted libraries at:
