import sqlite3

conn = sqlite3.connect('data/settings.db')

def main():
    c = conn.cursor()
    # c.execute('INSERT INTO settings VALUES ("screenCycle", "3500")')
    # conn.commit()
    conn.close()


if __name__ == '__main__':
    main()
