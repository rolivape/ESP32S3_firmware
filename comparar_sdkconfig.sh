#!/bin/bash

echo "Selecciona el primer archivo sdkconfig a comparar:"
read -e -p "Archivo 1: " archivo1
[ ! -f "$archivo1" ] && echo "El archivo $archivo1 no existe. Abortando." && exit 1

echo "Selecciona el segundo archivo sdkconfig a comparar:"
read -e -p "Archivo 2: " archivo2
[ ! -f "$archivo2" ] && echo "El archivo $archivo2 no existe. Abortando." && exit 1

# Filtra y ordena
grep '^CONFIG_' "$archivo1" | sort > /tmp/sdk1.conf
grep '^CONFIG_' "$archivo2" | sort > /tmp/sdk2.conf

echo
echo "Sólo líneas diferentes entre archivos:"
echo

# Muestra solo diferencias
comm -3 /tmp/sdk1.conf /tmp/sdk2.conf | less

rm /tmp/sdk1.conf /tmp/sdk2.conf
