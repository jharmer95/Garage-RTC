'''***************************************************************************
          File: sqliteReadAll.py
   Description: Fetches all data from the database.
       Authors: Daniel Zajac,  danzajac@umich.edu
                Jackson Harmer, harmer@umich.edu

***************************************************************************'''
import sqlite3

conn = sqlite3.connect('data/settings2.db')

def main():
    c = conn.cursor()
    c.execute('SELECT * FROM settings')
    vals = c.fetchall()
    for val in vals:
        print(val)
    conn.close()


if __name__ == '__main__':
    main()
