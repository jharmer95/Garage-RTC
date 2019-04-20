'''***************************************************************************
          File: sqliteWrite.py
   Description: Writes data back to the database.
       Authors: Daniel Zajac,  danzajac@umich.edu
                Jackson Harmer, harmer@umich.edu

***************************************************************************'''
import sqlite3

conn = sqlite3.connect('data/settings.db')

def main():
    inName = ''
    while True:
        inName = input('enter a name: ')
        if inName == '(exit)':
            break
        nameStr = inName
        inName = input('enter a value: ')
        if inName == '(exit)':
            break
        valStr = inName

        params = (nameStr, valStr)
        c = conn.cursor()
        c.execute('INSERT INTO settings VALUES ' + str(params))
        conn.commit()
        print('Name: "' + nameStr + '", Value: "' + valStr + '" - successfully added')
    conn.close()

if __name__ == '__main__':
    main()
