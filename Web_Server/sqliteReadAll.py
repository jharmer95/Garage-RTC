import sqlite3

conn = sqlite3.connect('data/settings.db')

def main():
    c = conn.cursor()
    c.execute('SELECT * FROM settings')
    vals = c.fetchall()
    for val in vals:
        print(val)
    conn.close()


if __name__ == '__main__':
    main()
