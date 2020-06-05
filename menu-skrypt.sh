#!/bin/bash

HEIGHT=15
WIDTH=40
CHOICE_HEIGHT=4
BACKTITLE="Radio"
TITLE2="Menu drugie"
TITLE="Menu początkowe"
MENU="Wybierz jedną z następujących opcji:"

OPTIONS=(1 "Szukaj pośrednika"
         2 "Koniec")

OPTIONS2=(1 "Szukaj pośrednika"
          2 "Pośrednik Radioaktywne"
          3 "Pośrednik Dobreradio"
          4 "Koniec")

MENUSTART=$(dialog --clear --backtitle "$BACKTITLE" \
                --title "$TITLE" --menu "$MENU" \
                $HEIGHT $WIDTH $CHOICE_HEIGHT \
                "${OPTIONS[@]}" 2>&1 >/dev/tty)
case $MENUSTART in
        1)
            echo "You chose Szukaj pośrednika"
            CHOICE=$(dialog --clear --backtitle "$BACKTITLE" \
                --title "$TITLE2" --menu "$MENU" \
                $HEIGHT $WIDTH $CHOICE_HEIGHT \
                "${OPTIONS2[@]}" 2>&1 >/dev/tty)
            case $CHOICE in
                    1)
                        echo "You chose Szukaj pośrednika";;
                    2)
                        echo "You chose Radioaktywne";;
                    3)
                        echo "You chose Dobreradio";;
                    4)
                        echo "You chose Koniec";;
            esac
            ;;
        2)
            echo "You chose Koniec";;
esac