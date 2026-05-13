from flask import Flask, render_template, request, jsonify
import requests
from bs4 import BeautifulSoup
import subprocess
import json
import os
import re
import unicodedata
from groq import Groq

app = Flask(__name__)

GROQ_API_KEY = "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"

def clean_text(text):
    text = unicodedata.normalize('NFKD', text).encode('ascii', 'ignore').decode('utf-8')
    text = re.sub(r'\s+', ' ', text)
    text = re.sub(r'[^a-zA-Z0-9\s.,!?\'"-]', '', text)
    return text.strip()

def scrape_wikipedia(url):
    try:
        headers = {'User-Agent': 'Mozilla/5.0'}
        response = requests.get(url, headers=headers, timeout=10)
        response.raise_for_status()
        soup = BeautifulSoup(response.text, 'html.parser')
        
        for table in soup.find_all(['table', 'div'], class_=['infobox', 'navbox', 'metadata', 'ombox', 'reflist']):
            table.decompose()
        for tag in soup(['script', 'style', 'nav', 'footer', 'header', 'aside', 'sup']):
            tag.decompose()
            
        raw_text = clean_text(soup.get_text(separator=' ', strip=True))
        with open("raw_data.txt", "w", encoding="utf-8") as f:
            f.write(raw_text)
        return raw_text
    except Exception as e:
        print(f"Scrape Error: {e}")
        return None

def generate_neuro_summary(text):
    try:
        client = Groq(api_key=GROQ_API_KEY)
        prompt = f"""You are the Neuro Scrape Intelligence System. 
        Summarize this C++ processed data into a deep, professional executive summary with bullet points.
        Format ONLY in valid HTML (<ul>, <li>, <p>, <strong>).
        DATA: {text[:30000]}"""
        
        chat_completion = client.chat.completions.create(
            messages=[{"role": "user", "content": prompt}],
            model="llama-3.3-70b-versatile",
            temperature=0.2,
        )
        return chat_completion.choices[0].message.content
    except Exception as e:
        return f"<p>Intelligence Extraction Failed: {e}</p>"

@app.route('/')
def index():
    return render_template('index.html')

@app.route('/api/analyze', methods=['POST'])
def analyze():
    data = request.json
    url = data.get('url')
    
    if "wikipedia.org" not in url.lower():
        return jsonify({"error": "Neuro Scrape AI is locked to Wikipedia domains."}), 400

    raw_text = scrape_wikipedia(url)
    if not raw_text:
        return jsonify({"error": "Failed to extract source data."}), 400

    # Run C++ Engine
    subprocess.run(["scraper.exe"])

    if os.path.exists("results.json"):
        with open("results.json", "r", encoding="utf-8") as f:
            results = json.load(f)
        
        results["processed_text"] = raw_text
        results["neuro_intelligence"] = generate_neuro_summary(raw_text)
        return jsonify(results)
    else:
        return jsonify({"error": "C++ Analytic Engine failed."}), 500

if __name__ == '__main__':
    app.run(debug=True, port=5000)