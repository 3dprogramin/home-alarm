// create an express app
const dotenv = require("dotenv");
dotenv.config();
const express = require("express");
const axios = require("axios");

const app = express();
const port = 3000;

const UPSTREAM_URL = process.env.UPSTREAM_URL;
const UPSTREAM_TOKEN = process.env.UPSTREAM_TOKEN;
const TOKEN = process.env.TOKEN;

// function that forwards the request to the upstream server
async function forwardRequest(path) {
  const url = `${UPSTREAM_URL}${path}?token=${UPSTREAM_TOKEN}`;
  try {
    const response = await axios.get(url);
    return response.data;
  } catch (error) {
    throw new Error(
      `Failed to forward request to upstream server: ${error.message}`
    );
  }
}

// middleware to handle /activate, /stop, /trigger routes
app.use(async (req, res, next) => {
  let upstreamStep = false;
  try {
    // make sure the request is having one of this pats
    const validPaths = ["/activate", "/stop", "/trigger"];
    if (!validPaths.includes(req.path)) {
      console.log(
        JSON.stringify({
          app: "home-alarm",
          level: "info",
          event: "request",
          path: req.path,
          message: "404 Not Found",
        })
      );
      return res.status(404).send("404 Not Found");
    }

    // check for token
    const token = req.query.token;
    if (TOKEN && token !== TOKEN) {
      console.log(
        JSON.stringify({
          app: "home-alarm",
          level: "info",
          event: "request",
          path: req.path,
          message: "Unauthorized access attempt",
        })
      );
      return res.status(403).send("Unauthorized");
    }

    console.log(
      JSON.stringify({
        app: "home-alarm",
        level: "info",
        event: "request",
        path: req.path,
      })
    );

    // respond with 200 OK for valid requests
    res.status(200).end();

    // forward the request to the upstream server
    upstreamStep = true;
    const upstreamResponse = await forwardRequest(req.path);
    console.log(
      JSON.stringify({
        app: "home-alarm",
        level: "info",
        event: "upstream_response",
        response: upstreamResponse,
        path: req.path,
      })
    );
  } catch (err) {
    console.log(
      JSON.stringify({
        app: "home-alarm",
        level: "error",
        event: upstreamStep ? "upstream_response" : "request",
        path: req.path,
      })
    );
    res.status(500).send(err.message || err);
  }
});

// start the express server
app.listen(port, () => {
  console.log(
    JSON.stringify({
      app: "home-alarm",
      level: "info",
      event: "init",
    })
  );
});
