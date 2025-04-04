from flask import Flask, jsonify, send_from_directory, send_file
import sqlite3
import csv
import io
from waitress import serve

app = Flask(__name__)
DB_NAME = "sensor_data.db"

def query_db(query, args=(), one=False):
    conn = sqlite3.connect(DB_NAME)
    conn.row_factory = sqlite3.Row
    cur = conn.cursor()
    cur.execute(query, args)
    rows = cur.fetchall()
    conn.close()
    result = [dict(row) for row in rows]
    return (result[0] if result else None) if one else result

# All historical data
@app.route("/data", methods=["GET"])
def get_all_data():
    rows = query_db("SELECT * FROM sensor_data ORDER BY id DESC")
    return jsonify(rows)

# Latest reading only
@app.route("/latest", methods=["GET"])
def get_latest():
    row = query_db("SELECT * FROM sensor_data ORDER BY id DESC LIMIT 1", one=True)
    return jsonify(row)

# Only alarm-triggered rows
@app.route("/alarm-data", methods=["GET"])  # Changed from /alarm to /alarm-data
def get_alarm_data():
    rows = query_db("SELECT * FROM sensor_data WHERE alarm_triggered = 1 ORDER BY id DESC")
    return jsonify(rows)

# Serve HTML pages
@app.route("/")
def index():
    return send_from_directory('static', "index.html")

@app.route("/history")
def history():
    return send_from_directory('static', 'history.html')

@app.route("/alarm")
def alarm_page():  # Changed function name from 'history' to 'alarm_page'
    return send_from_directory('static', 'alarm.html')

# CSV export
@app.route('/export')
def export_csv():
    conn = sqlite3.connect(DB_NAME)
    cursor = conn.cursor()
    cursor.execute("SELECT * FROM sensor_data ORDER BY id DESC")
    rows = cursor.fetchall()
    conn.close()

    # Create CSV in memory
    output = io.StringIO()
    writer = csv.writer(output)
    writer.writerow(['id', 'timestamp', 'distance', 'temperature', 'humidity', 'alarm_triggered'])
    writer.writerows(rows)

    output.seek(0)
    return send_file(
        io.BytesIO(output.getvalue().encode()),
        mimetype='text/csv',
        as_attachment=True,
        download_name='sensor_data.csv'
    )

if __name__ == "__main__":
    serve(app, host="0.0.0.0", port=5000)
